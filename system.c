#include <string.h>

#include "runtime/value.h"
#include "runtime/error.h"

#include "system.h"
#include "write.h"

static NseVal sum(NseVal args) {
  int64_t acc = 0;
  double facc = 0.0;
  int fp = 0;
  while (is_cons(args)) {
    NseVal h = head(args);
    if (h.type == i64_type) {
      acc += h.i64;
    } else if (h.type == f64_type) {
      facc += h.f64;
      fp = 1;
    } else {
      raise_error(domain_error, "expected number");
      return undefined;
    }
    args = tail(args);
  }
  if (fp) {
    return F64(acc + facc);
  }
  return I64(acc);
}

static NseVal subtract(NseVal args) {
  int64_t acc = 0;
  double facc = 0.0;
  int fp = 0;
  NseVal h = head(args);
  if (h.type == i64_type) {
    acc = h.i64;
  } else if (h.type == f64_type) {
    facc = h.f64;
    fp = 1;
  } else {
    raise_error(domain_error, "expected number");
    return undefined;
  }
  args = tail(args);
  if (!is_cons(args)) {
    if (fp) {
      return F64(-facc);
    }
    return I64(-acc);
  }
  do {
    h = head(args);
    if (h.type == i64_type) {
      acc -= h.i64;
    } else if (h.type == f64_type) {
      facc -= h.f64;
      fp = 1;
    } else {
      raise_error(domain_error, "expected number");
      return undefined;
    }
    args = tail(args);
  } while (is_cons(args));
  if (fp) {
    return F64(acc - facc);
  }
  return I64(acc);
}

static NseVal product(NseVal args) {
  int64_t acc = 1;
  double facc = 1.0;
  int fp = 0;
  while (is_cons(args)) {
    NseVal h = head(args);
    if (h.type == i64_type) {
      acc *= h.i64;
    } else if (h.type == f64_type) {
      facc *= h.f64;
      fp = 1;
    } else {
      raise_error(domain_error, "expected number");
      return undefined;
    }
    args = tail(args);
  }
  if (fp) {
    return F64(acc * facc);
  }
  return I64(acc);
}

static NseVal divide(NseVal args) {
  NseVal h = head(args);
  double acc;
  if (h.type == i64_type) {
    acc = h.i64;
  } else if (h.type == f64_type) {
    acc = h.f64;
  } else {
    raise_error(domain_error, "expected number");
    return undefined;
  }
  args = tail(args);
  if (!is_cons(args)) {
    return F64(1.0 / acc);
  }
  do {
    h = head(args);
    if (h.type == i64_type) {
      acc /= h.i64;
    } else if (h.type == f64_type) {
      acc /= h.f64;
    } else {
      raise_error(domain_error, "expected number");
      return undefined;
    }
    args = tail(args);
  } while (is_cons(args));
  return F64(acc);
}

static NseVal equals(NseVal args) {
  NseVal previous = undefined;
  while (args.type->internal == INTERNAL_CONS) {
    NseVal h = head(args);
    if (previous.type) {
      NseVal result = nse_equals(previous, h);
      if (!is_true(result)) {
        return FALSE;
      }
    }
    previous = h;
    args = tail(args);
  }
  if (!RESULT_OK(previous)) {
    raise_error(domain_error, "too few arguments");
  }
  return TRUE;
}

static NseVal apply(NseVal args) {
  ARG_POP_ANY(func, args);
  ARG_POP_ANY(func_args, args);
  ARG_DONE(args);
  return nse_apply(func, func_args);
}

static NseVal symbol_name(NseVal args) {
  ARG_POP_TYPE(Symbol *, symbol, args, to_symbol, "a symbol");
  ARG_DONE(args);
  return check_alloc(STRING(create_string(symbol->name, strlen(symbol->name))));
}

static NseVal symbol_module(NseVal args) {
  ARG_POP_TYPE(Symbol *, symbol, args, to_symbol, "a symbol");
  ARG_DONE(args);
  const char *name = module_name(symbol->module);
  return check_alloc(STRING(create_string(name, strlen(name))));
}

static NseVal module_symbols(NseVal args) {
  ARG_POP_ANY(arg, args);
  ARG_DONE(args);
  const char *name = to_string_constant(arg);
  if (name) {
    Module *m = find_module(name);
    if (m) {
      return list_external_symbols(m);
    } else {
      raise_error(name_error, "could not find module: %s", name);
    }
  } else {
    raise_error(domain_error, "must be called with a symbol");
  }
  return undefined;
}

static NseVal byte_length(NseVal args) {
  ARG_POP_TYPE(String *, string, args, to_string, "a string");
  ARG_DONE(args);
  return I64(string->length);
}

static NseVal byte_at(NseVal args) {
  ARG_POP_I64(n, args);
  ARG_POP_TYPE(String *, string, args, to_string, "a string");
  ARG_DONE(args);
  if (n >= string->length) {
    raise_error(domain_error, "string index out of bounds: %d", n);
    return undefined;
  }
  return I64((unsigned char) string->chars[n]);
}

static NseVal syntax_to_datum_(NseVal args) {
  ARG_POP_ANY(arg, args);
  ARG_DONE(args);
  return syntax_to_datum(arg);
}

static NseVal update_head(NseVal args) {
  ARG_POP_TYPE(Cons *, cons, args, to_cons, "a cons");
  ARG_POP_ANY(arg, args);
  ARG_DONE(args);
  if (cons->refs == 1) {
    NseVal old_head = cons->head;
    cons->head = add_ref(arg);
    add_ref(CONS(cons));
    del_ref(old_head);
  } else {
    cons = create_cons(arg, cons->tail);
  }
  return check_alloc(CONS(cons));
}

static NseVal type_of(NseVal args) {
  ARG_POP_ANY(arg, args);
  ARG_DONE(args);
  return TYPE(copy_type(arg.type));
}

static NseVal is_a(NseVal args) {
  NseVal a = head(args);
  NseVal b = elem(1, args);
  CType *type_a = a.type;
  NseVal result = undefined;
  if (type_a) {
    CType *type_b = to_type(b);
    if (type_b) {
      result = type_a == type_b ? TRUE : FALSE;
    }
  }
  return result;
}

Module *get_system_module() {
  Module *system = create_module("system");
  module_ext_define(system, "+", FUNC(sum, 0, 1));
  module_ext_define(system, "-", FUNC(subtract, 1, 1));
  module_ext_define(system, "*", FUNC(product, 0, 1));
  module_ext_define(system, "/", FUNC(divide, 1, 1));
  module_ext_define(system, "=", FUNC(equals, 1, 1));
  module_ext_define(system, "apply", FUNC(apply, 2, 0));
  module_ext_define(system, "symbol-name", FUNC(symbol_name, 1, 0));
  module_ext_define(system, "symbol-module", FUNC(symbol_module, 1, 0));
  module_ext_define(system, "module-symbols", FUNC(module_symbols, 1, 0));
  module_ext_define(system, "byte-length", FUNC(byte_length, 1, 0));
  module_ext_define(system, "byte-at", FUNC(byte_at, 2, 0));
  module_ext_define(system, "syntax->datum", FUNC(syntax_to_datum_, 1, 0));
  module_ext_define(system, "update-head", FUNC(update_head, 2, 0));
  module_ext_define(system, "is-a", FUNC(is_a, 2, 0));
  module_ext_define(system, "type-of", FUNC(type_of, 1, 0));

  module_ext_define_type(system, "any", TYPE(any_type));
  module_ext_define_type(system, "nil", TYPE(nil_type));
  module_ext_define_type(system, "i64", TYPE(i64_type));
  module_ext_define_type(system, "f64", TYPE(f64_type));
  module_ext_define_type(system, "string", TYPE(string_type));
  module_ext_define_type(system, "symbol", TYPE(symbol_type));
  module_ext_define_type(system, "type", TYPE(type_type));
  module_ext_define_type(system, "cons", TYPE(cons_type));
  return system;
}
