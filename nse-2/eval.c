#include <string.h>

#include "nsert.h"

#include "eval.h"

DEFINE_HASH_MAP(name_space, NameSpace, char *, NseVal *, string_hash, string_equals)

struct scope {
  size_t refs;
  NameSpace names;
  Scope *parent;
};

Scope *create_scope(Scope *parent) {
  Scope *scope = malloc(sizeof(Scope));
  scope->names = create_name_space();
  scope->parent = parent;
  scope->refs = 1;
  if (parent) {
    parent->refs++;
  }
  return scope;
}

void delete_scope(Scope *scope) {
  if (scope->refs > 1) {
    scope->refs--;
  } else {
    HashMapIterator *it = create_name_space_iterator(scope->names);
    for (HashMapEntry entry = next_entry(it); entry.key; entry = next_entry(it)) {
      if (entry.value) {
        del_ref(*(NseVal *)entry.value);
        free(entry.value);
      }
    }
    delete_hash_map_iterator(it);
    delete_name_space(scope->names);
    if (scope->parent) {
      delete_scope(scope->parent);
    }
    free(scope);
  }
}

NseVal scope_get(Scope *scope, const char *name) {
  NseVal *value = name_space_lookup(scope->names, name);
  if (value) {
    return *value;
  }
  if (scope->parent) {
    return scope_get(scope->parent, name);
  }
  raise_error("undefined name: %s", name);
  return undefined;
}

void scope_define(Scope *scope, const char *name, NseVal value) {
  NseVal *existing = name_space_remove(scope->names, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  name_space_add(scope->names, name, copy);
}

NseVal eval_list(NseVal list, Scope *scope) {
  if (list.type == TYPE_SYNTAX) {
    Syntax *previous = error_form;
    if (previous) add_ref(SYNTAX(previous));
    set_debug_form(list.syntax);
    NseVal result = eval_list(list.syntax->quoted, scope);
    if (result.type == TYPE_UNDEFINED) {
      return undefined;
    }
    set_debug_form(previous);
    if (previous) del_ref(SYNTAX(previous));
    return result;
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
  raise_error("invalid list");
  return undefined;
}

void assign_parameters(Scope *scope, NseVal formal, NseVal actual) {
  switch (formal.type) {
    case TYPE_SYNTAX:
      assign_parameters(scope, formal.syntax->quoted, actual);
      break;
    case TYPE_SYMBOL:
      scope_define(scope, formal.symbol, actual);
      break;
    case TYPE_CONS:
      assign_parameters(scope, head(formal), head(actual));
      assign_parameters(scope, tail(formal), tail(actual));
      break;
    case TYPE_NIL:
      // ok
      break;
    default:
      // not ok
      break;
  }
}

NseVal eval_anon(NseVal args, NseVal env[]) {
  NseVal definition = env[0];
  Scope *scope = env[1].reference->pointer;
  NseVal formal = head(tail(definition));
  NseVal body = head(tail(tail(definition)));
  assign_parameters(scope, formal, args);
  NseVal result = eval(body, scope);
  return result;
}

NseVal eval_cons(Cons *cons, Scope *scope) {
  NseVal operator = cons->head;
  NseVal args = cons->tail;
  if (is_symbol(operator, "if")) {
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
  } else if (is_symbol(operator, "let")) {
  } else if (is_symbol(operator, "fn")) {
    Scope *fn_scope = create_scope(scope);
    NseVal scope_ref = REFERENCE(create_reference(fn_scope, delete_scope));
    NseVal env[] = {args, scope_ref};
    NseVal result = CLOSURE(create_closure(eval_anon, env, 2));
    del_ref(scope_ref);
    return result;
  } else if (is_symbol(operator, "def")) {
    char *name = to_symbol(head(args));
    NseVal value = eval(head(tail(args)), scope);
    if (RESULT_OK(value)) {
      scope_define(scope, name, value);
      return SYMBOL(name);
    }
    return undefined;
  }
  NseVal result = undefined;
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
      return code;
    case TYPE_QUOTE:
      return code.quote->quoted;
    case TYPE_SYMBOL:
      return scope_get(scope, code.symbol);
    case TYPE_SYNTAX: {
      Syntax *previous = error_form;
      if (previous) add_ref(SYNTAX(previous));
      set_debug_form(code.syntax);
      NseVal result = eval(code.syntax->quoted, scope);
      if (result.type == TYPE_UNDEFINED) {
        return undefined;
      }
      set_debug_form(previous);
      if (previous) del_ref(SYNTAX(previous));
      return result;
    }
    default:
      return undefined;
  }
}
