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
DEFINE_HASH_MAP(symmap, char *, nse_val_t *)

hash_map *symbols = NULL;

void delete_all(nse_val_t prim);
void delete(nse_val_t prim);
nse_val_t parse_list(char **input);

nse_val_t Undefined = { .type = TYPE_UNDEFINED };
nse_val_t Nil = { .type = TYPE_NIL };

nse_val_t Cons(nse_val_t h, nse_val_t t) {
  nse_val_t prim;
  prim.type = TYPE_CONS;
  prim.value.cval = malloc(sizeof(cons_t));
  if (!prim.value.cval) {
    return Undefined;
  }
  prim.value.cval->refs = 1;
  prim.value.cval->h = h;
  prim.value.cval->t = t;
  printf("%zx[1]: cons\n", prim.value.cval);
  add_ref(h);
  add_ref(t);
  return prim;
}

nse_val_t Array(size_t size) {
  nse_val_t prim;
  prim.type = TYPE_ARRAY;
  prim.value.aval = malloc(sizeof(array_t) + size * sizeof(nse_val_t));
  if (!prim.value.aval) {
    return Undefined;
  }
  prim.value.aval->refs = 1;
  prim.value.aval->size = size;
  for (int i = 0; i < size; i++) {
    prim.value.aval->array[i] = Undefined;
  }
  return prim;
}

nse_val_t Quote(nse_val_t p) {
  nse_val_t prim;
  prim.type = TYPE_QUOTE;
  prim.value.qval = malloc(sizeof(quote_t));
  if (!prim.value.qval) {
    return Undefined;
  }
  prim.value.qval->refs = 1;
  prim.value.qval->quoted = p;
  printf("%zx[1]: quote\n", prim.value.qval);
  add_ref(p);
  return prim;
}

nse_val_t Int(int i) {
  return (nse_val_t) { .type = TYPE_INT, .value.ival = i };
}

nse_val_t Symbol(const char *s) {
  if (symbols == NULL) {
    symbols = hash_map_new();
  } else {
    nse_val_t *value = symmap_lookup(symbols, s);
    if (value != NULL) {
      return *value;
    }
  }
  size_t len = strlen(s);
  char *copy = (char *)malloc(len + 1);
  memcpy(copy, s, len);
  copy[len] = '\0';
  nse_val_t *sym = malloc(sizeof(nse_val_t));
  sym->type = TYPE_SYMBOL;
  sym->value.sval = copy;
  symmap_add(symbols, copy, sym);
  return *sym;
}

nse_val_t Func(nse_val_t f(nse_val_t)) {
  return (nse_val_t) { .type = TYPE_FUNC, .value.fval = f };
}

nse_val_t Closure(nse_val_t f(nse_val_t, nse_val_t[]), nse_val_t env[], size_t env_size) {
  closure_t *clos = malloc(sizeof(closure_t) + env_size * sizeof(nse_val_t));
  clos->refs = 1;
  clos->f = f;
  if (env_size > 0) {
    memcpy(clos->env, env, env_size * sizeof(nse_val_t));
  }
  printf("%zx[1]: closure\n", clos);
  return (nse_val_t) { .type = TYPE_CLOSURE, .value.clval = clos };
}

nse_val_t add_ref(nse_val_t prim) {
  switch (prim.type) {
    case  TYPE_CONS:
      printf("%zx[%zx]: ref++\n", prim.value.cval, prim.value.cval->refs + 1);
      prim.value.cval->refs++;
      break;
    case  TYPE_ARRAY:
      prim.value.aval->refs++;
      break;
    case  TYPE_CLOSURE:
      printf("%zx[%zx]: ref++\n", prim.value.clval, prim.value.clval->refs + 1);
      prim.value.clval->refs++;
      break;
    case  TYPE_QUOTE:
      printf("%zx[%zx]: ref++\n", prim.value.qval, prim.value.qval->refs + 1);
      prim.value.qval->refs++;
      break;
  }
  return prim;
}

void del_ref(nse_val_t prim) {
  size_t *refs = NULL;
  switch (prim.type) {
    case  TYPE_CONS:
      printf("%zx[%zx]: ref--\n", prim.value.cval, prim.value.cval->refs  - 1);
      refs = &prim.value.cval->refs;
      break;
    case  TYPE_ARRAY:
      refs = &prim.value.aval->refs;
      break;
    case  TYPE_CLOSURE:
      printf("%zx[%zx]: ref--\n", prim.value.clval, prim.value.clval->refs - 1);
      refs = &prim.value.clval->refs;
      break;
    case  TYPE_QUOTE:
      printf("%zx[%zx]: ref--\n", prim.value.qval, prim.value.qval->refs - 1);
      refs = &prim.value.qval->refs;
      break;
    default:
      return;
  }
  if (*refs > 0) {
    // error if already deleted?
    (*refs)--;
  }
  if (*refs == 0) {
    delete_all(prim);
  }
}

void delete_all(nse_val_t prim) {
  if (prim.type == TYPE_CONS) {
    del_ref(prim.value.cval->h);
    del_ref(prim.value.cval->t);
  }
  if (prim.type == TYPE_ARRAY) {
    for (int i = 0; i < prim.value.aval->size; i++) {
      del_ref(prim.value.aval->array[i]);
    }
  }
  if (prim.type == TYPE_QUOTE) {
    del_ref(prim.value.qval->quoted);
  }
  delete(prim);
}

void delete(nse_val_t prim) {
  switch (prim.type) {
    case TYPE_CONS:
      printf("%zx[%zx]: free cons\n", prim.value.cval, prim.value.cval->refs);
      free(prim.value.cval);
      return;
    case TYPE_ARRAY:
      free(prim.value.aval->array);
      free(prim.value.aval);
      return;
    case TYPE_SYMBOL:
      free(prim.value.sval);
      return;
    case TYPE_QUOTE:
      printf("%zx[%zx]: free quote\n", prim.value.qval, prim.value.qval->refs);
      free(prim.value.qval);
      return;
    case TYPE_CLOSURE:
      printf("%zx[%zx]: free closure\n", prim.value.clval, prim.value.clval->refs);
      free(prim.value.clval);
      return;
    default:
      return;
  }
}

nse_val_t head(nse_val_t cons) {
  nse_val_t result = Undefined;
  if (cons.type == TYPE_CONS) {
    result = cons.value.cval->h;
  }
  return result;
}

nse_val_t tail(nse_val_t cons) {
  nse_val_t result = Undefined;
  if (cons.type == TYPE_CONS) {
    result = cons.value.cval->t;
  }
  return result;
}

int is_symbol(nse_val_t v, const char *sym) {
  int result = 0;
  if (v.type == TYPE_SYMBOL) {
    result = strcmp(v.value.sval, sym) == 0;
  }
  return result;
}

int is_true(nse_val_t b) {
  return is_symbol(b, "t");
}

size_t list_length(nse_val_t cons) {
  size_t count = 0;
  while (cons.type == TYPE_CONS) {
    count++;
    cons = cons.value.cval->t;
  }
  return count;
}

nse_val_t nse_apply(nse_val_t func, nse_val_t args) {
  nse_val_t result = Undefined;
  if (func.type == TYPE_FUNC) {
    result = func.value.fval(args);
  } else if (func.type == TYPE_CLOSURE) {
    result = func.value.clval->f(args, func.value.clval->env);
  }
  return result;
}

nse_val_t nse_and(nse_val_t a, nse_val_t b) {
  if (is_true(a) && is_true(b)) {
    return Symbol("t");
  }
  return Symbol("f");
}

nse_val_t nse_equals(nse_val_t a, nse_val_t b) {
  if (a.type == TYPE_UNDEFINED || b.type == TYPE_UNDEFINED) {
    return Undefined;
  }
  if (a.type != b.type) {
    return Symbol("f");
  }
  switch (a.type) {
    case TYPE_NIL:
      return Symbol("t");
    case TYPE_CONS:
      return nse_and(nse_equals(head(a), head(b)), nse_equals(tail(a), tail(b)));
    case TYPE_ARRAY:
      if (a.value.aval->size != b.value.aval->size) {
        return Symbol("f");
      }
      for (int i = 0; i < a.value.aval->size; i++) {
        if (!is_true(nse_equals(a.value.aval->array[i], b.value.aval->array[i]))) {
          return Symbol("f");
        }
      }
      return Symbol("t");
    case TYPE_SYMBOL:
      if (a.value.sval == b.value.sval) {
        return Symbol("t");
      }
      return Symbol("f");
    case TYPE_QUOTE:
      return nse_equals(a.value.qval->quoted, b.value.qval->quoted);
    case TYPE_INT:
      if (a.value.ival == b.value.ival) {
        return Symbol("t");
      }
      return Symbol("f");
    default:
      return Symbol("f");
  }
}

void print_cons(cons_t *cons) {
    nse_print(cons->h);
    if (cons->t.type == TYPE_CONS) {
      printf(" ");
      print_cons(cons->t.value.cval);
    } else if (cons->t.type != TYPE_NIL) {
      printf(" . ");
      nse_print(cons->t);
    }
}

nse_val_t nse_print(nse_val_t prim) {
  switch (prim.type) {
    case TYPE_NIL:
      printf("()");
      break;
    case TYPE_CONS:
      printf("(");
      print_cons(prim.value.cval);
      printf(")");
      break;
    case TYPE_SYMBOL:
      printf("%s", prim.value.sval);
      break;
    case TYPE_INT:
      printf("%d", prim.value.ival);
      break;
    case TYPE_QUOTE:
      printf("'");
      nse_print(prim.value.qval->quoted);
      break;
    default:
      printf("print error: undefined type %d\n", prim.type);
  }
  return Nil;
}
