#ifndef NSERT_H
#define NSERT_H

#include <stdlib.h>

#define TYPE_UNDEFINED 0
#define TYPE_NIL 1
#define TYPE_CONS 2
#define TYPE_INT 3
#define TYPE_SYMBOL 4
#define TYPE_QUOTE 5
#define TYPE_ARRAY 6
#define TYPE_FUNC 7
#define TYPE_CLOSURE 8

#define FUNC(f) ((nse_val_t) { .type = TYPE_FUNC, .value.fval = (f) })

typedef struct nse_val nse_val_t;
typedef struct cons cons_t;
typedef struct array array_t;
typedef struct closure closure_t;
typedef struct quote quote_t;

struct nse_val {
  unsigned char type;
  union {
    int ival;
    cons_t *cval;
    array_t *aval;
    quote_t *qval;
    char *sval;
    nse_val_t (*fval)(nse_val_t);
    closure_t *clval;
  } value;
};

struct array {
  size_t refs;
  size_t size;
  nse_val_t array[];
};

struct cons {
  size_t refs;
  nse_val_t h;
  nse_val_t t;
};

struct closure {
  size_t refs;
  nse_val_t (*f)(nse_val_t, nse_val_t[]);
  nse_val_t env[];
};

struct quote {
  size_t refs;
  nse_val_t quoted;
};

extern nse_val_t Undefined;
extern nse_val_t Nil;
nse_val_t Cons(nse_val_t h, nse_val_t t);
nse_val_t Array(size_t size);
nse_val_t Quote(nse_val_t p);
nse_val_t Int(int i);
nse_val_t Symbol(const char *s);
nse_val_t Func(nse_val_t f(nse_val_t));
nse_val_t Closure(nse_val_t f(nse_val_t, nse_val_t[]), nse_val_t env[], size_t env_size);

nse_val_t add_ref(nse_val_t p);
void del_ref(nse_val_t p);

nse_val_t head(nse_val_t cons);
nse_val_t tail(nse_val_t cons);
size_t list_length(nse_val_t cons);

int is_symbol(nse_val_t v, const char *sym);
int is_true(nse_val_t b);
nse_val_t nse_apply(nse_val_t func, nse_val_t args);
nse_val_t nse_and(nse_val_t a, nse_val_t b);
nse_val_t nse_equals(nse_val_t a, nse_val_t b);

nse_val_t nse_print(nse_val_t prim);

#endif
