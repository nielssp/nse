#include <string.h>

#include "nsert.h"

#include "eval.h"

size_t name_space_hash(const void *p) {
  const char *key = (const char *)p;
  size_t hash = 0;
  while (*key) {
    hash ^= *(key++);
  }
  return hash;
}
int name_space_equals(const void *a, const void *b) {
  return strcmp(a, b) == 0;
}
DEFINE_HASH_MAP(name_space, NameSpace, char *, NseVal *)

struct scope {
  NameSpace names;
  Scope *parent;
};

Scope *create_scope() {
  Scope *scope = malloc(sizeof(Scope));
  scope->names = create_name_space();
  scope->parent = NULL;
  return scope;
}

void delete_scope(Scope *scope) {
  HashMapIterator *it = create_name_space_iterator(scope->names);
  for (HashMapEntry entry = next_entry(it); entry.key; entry = next_entry(it)) {
    if (entry.value) {
      del_ref(*(NseVal *)entry.value);
      free(entry.value);
    }
  }
  delete_hash_map_iterator(it);
  delete_name_space(scope->names);
  free(scope);
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
  NseVal *existing = name_space_lookup(scope->names, name);
  if (existing) {
    // redefining...
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
    NseVal head = eval(list.cons->head, scope);
    NseVal tail = eval_list(list.cons->tail, scope);
    Cons *cons = create_cons(head, tail);
    del_ref(head);
    del_ref(tail);
    return CONS(cons);
  }
  raise_error("invalid list");
  return undefined;
}

NseVal eval_cons(Cons *cons, Scope *scope) {
  NseVal operator = cons->head;
  NseVal args = cons->tail;
  if (is_symbol(operator, "if")) {
    NseVal condition = eval(head(args), scope);
    NseVal result;
    if (is_true(condition)) {
      result = eval(head(tail(args)), scope);
    } else {
      result = eval(head(tail(tail(args))), scope);
    }
    del_ref(condition);
    return result;
  } else if (is_symbol(operator, "let")) {
  } else if (is_symbol(operator, "def")) {
    char *name = to_symbol(head(args));
    NseVal value = eval(head(tail(args)), scope);
    scope_define(scope, name, value);
    return SYMBOL(name);
  }
  NseVal function = eval(operator, scope);
  NseVal arg_list = eval_list(args, scope);
  NseVal result = nse_apply(function, arg_list);
  del_ref(function);
  del_ref(arg_list);
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
