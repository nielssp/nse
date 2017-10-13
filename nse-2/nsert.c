#include <stdio.h>
#include <string.h>

#include "nsert.h"
#include "util/hash_map.h"

size_t symmap_hash(const void *p) {
  const char *key = (const char *)p;
  size_t hash = 0;
  while (*key) {
    hash ^= *(key++);
  }
  return hash;
}
int symmap_equals(const void *a, const void *b) {
  return strcmp(a, b) == 0;
}
DEFINE_HASH_MAP(symmap, char *, char *)

hash_map *symbols = NULL;

void delete_all(NseVal value);
void delete(NseVal value);
NseVal parse_list(char **input);

NseVal undefined = { .type = TYPE_UNDEFINED };
NseVal nil = { .type = TYPE_NIL };

Cons *create_cons(NseVal h, NseVal t) {
  Cons *cons = malloc(sizeof(Cons));
  if (!cons) {
    return NULL;
  }
  cons->refs = 1;
  cons->head = h;
  cons->tail = t;
  add_ref(h);
  add_ref(t);
  return cons;
}

Quote *create_quote(NseVal quoted) {
  Quote *quote = malloc(sizeof(Quote));
  if (!quote) {
    return NULL;
  }
  quote->refs = 1;
  quote->quoted = quoted;
  add_ref(quoted);
  return quote;
}

Syntax *create_syntax(NseVal quoted) {
  Syntax *syntax = malloc(sizeof(Syntax));
  if (!syntax) {
    return NULL;
  }
  syntax->refs = 1;
  syntax->quoted = quoted;
  add_ref(quoted);
  return syntax;
}

Symbol *create_symbol(const char *s) {
  if (symbols == NULL) {
    symbols = hash_map_new();
  } else {
    Symbol *value = symmap_lookup(symbols, s);
    if (value != NULL) {
      return value;
    }
  }
  size_t len = strlen(s);
  char *copy = (char *)malloc(len + 1);
  memcpy(copy, s, len);
  copy[len] = '\0';
  symmap_add(symbols, copy, copy);
  return copy;
}

Closure *crate_closure(NseVal f(NseVal, NseVal[]), NseVal env[], size_t env_size) {
  Closure *closure = malloc(sizeof(Closure) + env_size * sizeof(NseVal));
  closure->refs = 1;
  closure->f = f;
  if (env_size > 0) {
    memcpy(closure->env, env, env_size * sizeof(NseVal));
  }
  return closure;
}

NseVal add_ref(NseVal value) {
  switch (value.type) {
    case  TYPE_CONS:
      value.cons->refs++;
      break;
    case  TYPE_CLOSURE:
      value.closure->refs++;
      break;
    case  TYPE_QUOTE:
      value.quote->refs++;
      break;
    case  TYPE_SYNTAX:
      value.syntax->refs++;
      break;
  }
  return value;
}

void del_ref(NseVal value) {
  size_t *refs = NULL;
  switch (value.type) {
    case  TYPE_CONS:
      refs = &value.cons->refs;
      break;
    case  TYPE_CLOSURE:
      refs = &value.closure->refs;
      break;
    case  TYPE_QUOTE:
      refs = &value.quote->refs;
      break;
    case  TYPE_SYNTAX:
      refs = &value.syntax->refs;
      break;
    default:
      return;
  }
  if (*refs > 0) {
    // error if already deleted?
    (*refs)--;
  }
  if (*refs == 0) {
    delete_all(value);
  }
}

void delete_all(NseVal value) {
  if (value.type == TYPE_CONS) {
    del_ref(value.cons->head);
    del_ref(value.cons->tail);
  }
  if (value.type == TYPE_QUOTE) {
    del_ref(value.quote->quoted);
  }
  if (value.type == TYPE_SYNTAX) {
    del_ref(value.syntax->quoted);
  }
  delete(value);
}

void delete(NseVal value) {
  switch (value.type) {
    case TYPE_CONS:
      free(value.cons);
      return;
    case TYPE_SYMBOL:
      free(value.symbol);
      return;
    case TYPE_QUOTE:
      free(value.quote);
      return;
    case TYPE_SYNTAX:
      free(value.syntax);
      return;
    case TYPE_CLOSURE:
      free(value.closure);
      return;
    default:
      return;
  }
}

NseVal head(NseVal value) {
  NseVal result = undefined;
  if (value.type == TYPE_CONS) {
    result = value.cons->head;
  } else {
    // TODO: type error
  }
  return result;
}

NseVal tail(NseVal value) {
  NseVal result = undefined;
  if (value.type == TYPE_CONS) {
    result = value.cons->tail;
  }
  return result;
}

int is_symbol(NseVal v, const char *sym) {
  int result = 0;
  if (v.type == TYPE_SYMBOL) {
    result = strcmp(v.symbol, sym) == 0;
  }
  return result;
}

int is_true(NseVal b) {
  return is_symbol(b, "t");
}

size_t list_length(NseVal value) {
  size_t count = 0;
  while (value.type == TYPE_CONS) {
    count++;
    value = value.cons->tail;
  }
  return count;
}

NseVal nse_apply(NseVal func, NseVal args) {
  NseVal result = undefined;
  if (func.type == TYPE_FUNC) {
    result = func.func(args);
  } else if (func.type == TYPE_CLOSURE) {
    result = func.closure->f(args, func.closure->env);
  }
  return result;
}

NseVal nse_and(NseVal a, NseVal b) {
  if (is_true(a) && is_true(b)) {
    return TRUE;
  }
  return FALSE;
}

NseVal nse_equals(NseVal a, NseVal b) {
  if (a.type == TYPE_UNDEFINED || b.type == TYPE_UNDEFINED) {
    return undefined;
  }
  if (a.type != b.type) {
    return FALSE;
  }
  switch (a.type) {
    case TYPE_NIL:
      return b.type == TYPE_NIL ? TRUE : FALSE;
    case TYPE_CONS:
      return nse_and(nse_equals(head(a), head(b)), nse_equals(tail(a), tail(b)));
    case TYPE_SYMBOL:
      if (a.symbol == b.symbol) {
        return TRUE;
      }
      return FALSE;
    case TYPE_QUOTE:
      return nse_equals(a.quote->quoted, b.quote->quoted);
    case TYPE_SYNTAX:
      return nse_equals(a.syntax->quoted, b.syntax->quoted);
    case TYPE_I64:
      if (a.i64 == b.i64) {
        return TRUE;
      }
      return FALSE;
    default:
      return FALSE;
  }
}

void print_cons(Cons *cons) {
    print(cons->head);
    if (cons->tail.type == TYPE_CONS) {
      printf(" ");
      print_cons(cons->tail.cons);
    } else if (cons->tail.type != TYPE_NIL) {
      printf(" . ");
      print(cons->tail);
    }
}

NseVal print(NseVal value) {
  switch (value.type) {
    case TYPE_NIL:
      printf("()");
      break;
    case TYPE_CONS:
      printf("(");
      print_cons(value.cons);
      printf(")");
      break;
    case TYPE_SYMBOL:
      printf("%s", value.symbol);
      break;
    case TYPE_I64:
      printf("%ld", value.i64);
      break;
    case TYPE_QUOTE:
      printf("'");
      print(value.quote->quoted);
      break;
    default:
      printf("print error: undefined type %d\n", value.type);
  }
  return nil;
}
