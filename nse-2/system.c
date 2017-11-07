#include "nsert.h"

#include "system.h"
#include "write.h"

static NseVal sum(NseVal args) {
  int64_t acc = 0;
  while (is_cons(args)) {
    acc += head(args).i64;
    args = tail(args);
  }
  return I64(acc);
}

static NseVal subtract(NseVal args) {
  int64_t acc = head(args).i64;
  args = tail(args);
  if (!is_cons(args)) {
    return I64(-acc);
  }
  do {
    acc -= head(args).i64;
    args = tail(args);
  } while (is_cons(args));
  return I64(acc);
}

static NseVal product(NseVal args) {
  int64_t acc = 1;
  while (is_cons(args)) {
    acc *= head(args).i64;
    args = tail(args);
  }
  return I64(acc);
}

static NseVal divide(NseVal args) {
  double acc = head(args).i64;
  args = tail(args);
  if (!is_cons(args)) {
    return F64(1.0 / acc);
  }
  do {
    acc /= head(args).i64;
    args = tail(args);
  } while (is_cons(args));
  return F64(acc);
}

static NseVal type_of(NseVal args) {
  Type *t = get_type(head(args));
  return check_alloc(TYPE(t));
}

static NseVal equals(NseVal args) {
  NseVal previous = undefined;
  while (args.type == TYPE_CONS) {
    NseVal h = head(args);
    if (previous.type != TYPE_UNDEFINED) {
      NseVal result = nse_equals(previous, h);
      if (!is_true(result)) {
        return FALSE;
      }
    }
    previous = h;
    args = tail(args);
  }
  if (!RESULT_OK(previous)) {
    raise_error("too few arguments");
  }
  return TRUE;
}

static NseVal is_a(NseVal args) {
  NseVal a = head(args);
  NseVal b = elem(1, args);
  Type *type_a = get_type(a);
  NseVal result = undefined;
  if (type_a) {
    Type *type_b = to_type(b);
    if (type_b) {
      result = is_subtype_of(type_a, type_b) ? TRUE : FALSE;
    }
    delete_type(type_a);
  }
  return result;
}

static NseVal subtype_of(NseVal args) {
  NseVal a = head(args);
  NseVal b = elem(1, args);
  Type *type_a = to_type(a);
  Type *type_b = to_type(b);
  if (type_a && type_b) {
    return is_subtype_of(type_a, type_b) ? TRUE : FALSE;
  }
  return undefined;
}

static NseVal cons_type(NseVal args) {
//  DESCRIBE_FORM("(cons a b)");
//  DESCRIBE_FUNC("Creates a cons-type from types a and b");
//  DESCRIBE_TYPE("&(-> (* type type) type)");
  ARG_POP_TYPE(Type *, type_a, args, to_type, "a type");
  ARG_POP_TYPE(Type *, type_b, args, to_type, "a type");
  ARG_DONE(args);
  return TYPE(create_cons_type(copy_type(type_a), copy_type(type_b)));
}

static NseVal simplify_type_(NseVal args) {
  ARG_POP_TYPE(Type *, type_a, args, to_type, "a type");
  ARG_DONE(args);
  return TYPE(simplify_type(type_a));
}

static NseVal union_type(NseVal args) {
  NseVal a = head(args);
  NseVal b = elem(1, args);
  Type *type_a = to_type(a);
  Type *type_b = to_type(b);
  if (type_a && type_b) {
    return TYPE(create_union_type(copy_type(type_a), copy_type(type_b)));
  }
  return undefined;
}

static NseVal func_type(NseVal args) {
  ARG_POP_TYPE(Type *, type_a, args, to_type, "a type");
  ARG_POP_TYPE(Type *, type_b, args, to_type, "a type");
  ARG_DONE(args);
  return TYPE(create_func_type(copy_type(type_a), copy_type(type_b)));
}

static NseVal forall_type(NseVal args) {
  ARG_POP_TYPE(Symbol *, symbol, args, to_symbol, "a symbol");
  ARG_POP_TYPE(Type *, t, args, to_type, "a type");
  ARG_DONE(args);
  return TYPE(create_forall_type(symbol->name, copy_type(t)));
}

static NseVal expand_type(NseVal args) {
  NseVal a = head(args);
  Type *type_a = to_type(a);
  if (type_a) {
    if (type_a->type == BASE_TYPE_ALIAS) {
      return TYPE(copy_type(type_a->param_b));
    }
    return a;
  }
  return undefined;
}

static NseVal apply(NseVal args) {
  ARG_POP_ANY(func, args);
  ARG_POP_ANY(func_args, args);
  ARG_DONE(args);
  return nse_apply(func, func_args);
}

Module *get_system_module() {
  Module *system = create_module("system");
  module_ext_define(system, "+", FUNC(sum));
  module_ext_define(system, "-", FUNC(subtract));
  module_ext_define(system, "*", FUNC(product));
  module_ext_define(system, "/", FUNC(divide));
  module_ext_define(system, "=", FUNC(equals));
  module_ext_define(system, "apply", FUNC(apply));

  module_ext_define(system, "type-of", FUNC(type_of));
  module_ext_define(system, "is-a", FUNC(is_a));
  module_ext_define(system, "subtype-of?", FUNC(subtype_of));
  module_ext_define(system, "cons-type", FUNC(cons_type));
  module_ext_define(system, "union-type", FUNC(union_type));
  module_ext_define(system, "expand-type", FUNC(expand_type));
  module_ext_define(system, "simplify-type", FUNC(simplify_type_));

  module_ext_define_type(system, "nothing", TYPE(nothing_type));
  module_ext_define_type(system, "any", TYPE(any_type));
  module_ext_define_type(system, "nil", TYPE(nil_type));
  module_ext_define_type(system, "ref", TYPE(ref_type));
  module_ext_define_type(system, "i8", TYPE(i8_type));
  module_ext_define_type(system, "i16", TYPE(i16_type));
  module_ext_define_type(system, "i32", TYPE(i32_type));
  module_ext_define_type(system, "i64", TYPE(i64_type));
  module_ext_define_type(system, "u8", TYPE(u8_type));
  module_ext_define_type(system, "u16", TYPE(u16_type));
  module_ext_define_type(system, "u32", TYPE(u32_type));
  module_ext_define_type(system, "u64", TYPE(u64_type));
  module_ext_define_type(system, "f32", TYPE(f32_type));
  module_ext_define_type(system, "f64", TYPE(f64_type));
  module_ext_define_type(system, "string", TYPE(string_type));
  module_ext_define_type(system, "any-symbol", TYPE(any_symbol_type));
  module_ext_define_type(system, "type", TYPE(type_type));
  module_ext_define_type(system, "cons", FUNC(cons_type));
  module_ext_define_type(system, "+", FUNC(union_type));
  //module_ext_define_type(system, "*", FUNC(product_type));
  module_ext_define_type(system, "->", FUNC(func_type));
  module_ext_define_type(system, "forall", FUNC(forall_type));
  return system;
}
