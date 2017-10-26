#include <string.h>

#include "nsert.h"

#include "eval.h"

DEFINE_HASH_MAP(namespace, Namespace, char *, NseVal *, string_hash, string_equals)

struct module {
  const char *name;
  Namespace internal;
  Namespace internal_macros;
  Namespace internal_types;
};

struct scope {
  Module *module;
  const char *name;
  NseVal value;
  Scope *next;
};

Scope *scope_push(Scope *next, const char *name, NseVal value) {
  Scope *scope = malloc(sizeof(Scope));
  scope->name = name;
  scope->value = value;
  scope->next = next;
  if (next) {
    scope->module = next->module;
  }
  add_ref(value);
  return scope;
}

Scope *scope_pop(Scope *scope) {
  Scope *next = scope->next;
  del_ref(scope->value);
  free(scope);
  return next;
}

void scope_pop_until(Scope *start, Scope *end) {
  while (start != end) {
    Scope *next = start->next;
    del_ref(start->value);
    free(start);
    start = next;
  }
}

Scope *copy_scope(Scope *scope) {
  if (scope == NULL) {
    return NULL;
  }
  Scope *copy = malloc(sizeof(Scope));
  copy->name = scope->name;
  copy->value = scope->value;
  copy->next = copy_scope(scope->next);
  copy->module = scope->module;
  add_ref(copy->value);
  return copy;
}

void delete_scope(Scope *scope) {
  if (scope != NULL) {
    delete_scope(scope->next);
    del_ref(scope->value);
    free(scope);
  }
}

NseVal scope_get(Scope *scope, const char *name) {
  if (scope->name) {
    if (strcmp(name, scope->name) == 0) {
      return scope->value;
    }
    if (scope->next) {
      return scope_get(scope->next, name);
    }
  }
  if (scope->module) {
    NseVal *value = namespace_lookup(scope->module->internal, name);
    if (value) {
      return *value;
    }
  }
  raise_error("undefined name");
  return undefined;
}

Module *create_module(const char *name) {
  Module *module = malloc(sizeof(Module));
  module->name = name;
  module->internal = create_namespace();
  module->internal_macros = create_namespace();
  module->internal_types = create_namespace();
  return module;
}

void delete_names(Namespace namespace) {
  NamespaceIterator it = create_namespace_iterator(namespace);
  for (NamespaceEntry entry = namespace_next(it); entry.key; entry = namespace_next(it)) {
    if (entry.value) {
      del_ref(*entry.value);
      free(entry.value);
    }
  }
  delete_namespace_iterator(it);
  delete_namespace(namespace);
}

void delete_module(Module *module) {
  delete_names(module->internal);
  delete_names(module->internal_macros);
  delete_names(module->internal_types);
  free(module);
}

Scope *use_module(Module *module) {
  Scope *scope = scope_push(NULL, NULL, undefined);
  scope->module = module;
  return scope;
}

void module_define(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->internal, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->internal, name, copy);
  add_ref(value);
}

void module_define_macro(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->internal_macros, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->internal_macros, name, copy);
  add_ref(value);
}

void module_define_type(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->internal_types, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->internal_types, name, copy);
  add_ref(value);
}

NseVal eval_list(NseVal list, Scope *scope) {
  if (list.type == TYPE_SYNTAX) {
    Syntax *previous = push_debug_form(list.syntax);
    return pop_debug_form(eval_list(list.syntax->quoted, scope), previous);
  } else if (list.type == TYPE_NIL) {
    return nil;
  } else if (list.type == TYPE_CONS) {
    NseVal result = undefined;
    NseVal head = eval(list.cons->head, scope);
    if (RESULT_OK(head)) {
      NseVal tail = eval_list(list.cons->tail, scope);
      if (RESULT_OK(tail)) {
        Cons *cons = create_cons(head, tail);
        if (cons != NULL) {
          result = CONS(cons);
        }
        del_ref(tail);
      }
      del_ref(head);
    }
    return result;
  }
  return eval(list, scope);
}

int assign_parameters(Scope **scope, NseVal formal, NseVal actual) {
  switch (formal.type) {
    case TYPE_SYNTAX:
      if (assign_parameters(scope, formal.syntax->quoted, actual)) {
        return 1;
      } else {
        return 0;
      }
    case TYPE_SYMBOL:
      *scope = scope_push(*scope, formal.symbol, actual);
      return 1;
    case TYPE_QUOTE:
      if (!is_true(nse_equals(formal.quote->quoted, actual))) {
        raise_error("pattern match failed");
        return 0;
      }
      return 1;
    case TYPE_CONS:
      if (!is_cons(actual)) {
        raise_error("too few parameters for function");
        return 0;
      }
      return assign_parameters(scope, head(formal), head(actual))
        && assign_parameters(scope, tail(formal), tail(actual));
    case TYPE_NIL:
      if (!is_nil(actual)) {
        raise_error("too many parameters for function");
        return 0;
      }
      return 1;
    default:
      // not ok
      return 0;
  }
}

NseVal eval_anon(NseVal args, NseVal env[]) {
  NseVal definition = env[0];
  Scope *scope = env[1].reference->pointer;
  NseVal formal = head(definition);
  NseVal body = head(tail(definition));
  Scope *current_scope = scope;
  NseVal result = undefined;
  if (assign_parameters(&current_scope, formal, args)) {
    result = eval(body, current_scope);
  }
  scope_pop_until(current_scope, scope);
  return result;
}

NseVal eval_cons(Cons *cons, Scope *scope) {
  NseVal operator = cons->head;
  NseVal args = cons->tail;
  if (match_symbol(operator, "if")) {
    NseVal result = undefined;
    NseVal condition = eval(head(args), scope);
    if (RESULT_OK(condition)) {
      if (is_true(condition)) {
        result = eval(head(tail(args)), scope);
      } else {
        result = eval(head(tail(tail(args))), scope);
      }
      del_ref(condition);
    }
    return result;
  } else if (match_symbol(operator, "let")) {
  } else if (match_symbol(operator, "fn")) {
    Scope *fn_scope = copy_scope(scope);
    NseVal result = undefined;
    NseVal scope_ref = check_alloc(REFERENCE(create_reference(fn_scope, (Destructor) delete_scope)));
    if (RESULT_OK(scope_ref)) {
      NseVal env[] = {args, scope_ref};
      result = check_alloc(CLOSURE(create_closure(eval_anon, env, 2)));
      del_ref(scope_ref);
    }
    return result;
  } else if (match_symbol(operator, "def")) {
    char *name = to_symbol(head(args));
    if (name) {
      NseVal value = eval(head(tail(args)), scope);
      if (RESULT_OK(value)) {
        module_define(scope->module, name, value);
        del_ref(value);
        return SYMBOL(name);
      }
    } else {
      raise_error("name must be a symbol");
    }
    return undefined;
  } else if (match_symbol(operator, "defm")) {
    char *name = to_symbol(head(args));
    if (name) {
      Scope *macro_scope = copy_scope(scope);
      NseVal scope_ref = check_alloc(REFERENCE(create_reference(macro_scope, (Destructor) delete_scope)));
      if (RESULT_OK(scope_ref)) {
        NseVal env[] = {tail(args), scope_ref};
        NseVal value = check_alloc(CLOSURE(create_closure(eval_anon, env, 2)));
        del_ref(scope_ref);
        if (RESULT_OK(value)) {
          module_define_macro(scope->module, name, value);
          del_ref(value);
          return SYMBOL(name);
        }
      }
    } else {
      raise_error("name must be a symbol");
    }
    return undefined;
  }
  NseVal result = undefined;
  if (is_symbol(operator)) {
    NseVal *macro_function = namespace_lookup(scope->module->internal_macros, to_symbol(operator));
    if (macro_function) {
      NseVal expanded = nse_apply(*macro_function, args);
      if (!RESULT_OK(expanded)) {
        return expanded;
      }
      result = eval(expanded, scope);
      del_ref(expanded);
      return result;
    }
  }
  NseVal function = eval(operator, scope);
  if (RESULT_OK(function)) {
    NseVal arg_list = eval_list(args, scope);
    if (RESULT_OK(arg_list)) {
      result = nse_apply(function, arg_list);
      del_ref(arg_list);
    }
    del_ref(function);
  }
  return result;
}

NseVal eval(NseVal code, Scope *scope) {
  switch (code.type) {
    case TYPE_CONS:
      return eval_cons(code.cons, scope);
    case TYPE_I64:
    case TYPE_STRING:
      return add_ref(code);
    case TYPE_QUOTE:
      return syntax_to_datum(code.quote->quoted);
    case TYPE_SYMBOL:
      return add_ref(scope_get(scope, code.symbol));
    case TYPE_SYNTAX: {
      Syntax *previous = push_debug_form(code.syntax);
      return pop_debug_form(eval(code.syntax->quoted, scope), previous);
    }
    default:
      raise_error("unexpected value type: %s", nse_val_type_to_string(code.type));
      return undefined;
  }
}

NseVal expand_macro_1(NseVal code, Scope *scope, int *expanded) {
  if (!scope->module || !is_cons(code)) {
    return code;
  }
  NseVal macro = head(code);
  if (!is_symbol(macro)) {
    return code;
  }
  if (is_special_form(macro) ) {
    return code;
  }
  NseVal args = tail(code);
  NseVal *function = namespace_lookup(scope->module->internal_macros, to_symbol(macro));
  if (!function) {
    return code;
  }
  *expanded = 1;
  return nse_apply(*function, args);
}

NseVal expand_macro(NseVal code, Scope *scope) {
  int expanded;
  do {
    expanded = 0;
    NseVal result = expand_macro_1(code, scope, &expanded);
    del_ref(code);
    code = result;
  } while (expanded && RESULT_OK(code));
  return code;
}
