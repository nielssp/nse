#ifndef NSERT_H
#define NSERT_H

#include <stdlib.h>

#define TYPE_UNDEFINED 0
#define TYPE_NIL 1
#define TYPE_CONS 2
#define TYPE_I64 3
#define TYPE_SYMBOL 4
#define TYPE_QUOTE 5
#define TYPE_TYPE_QUOTE 6
#define TYPE_SYNTAX 7
#define TYPE_FUNC 8
#define TYPE_CLOSURE 9

#define I64(i) ((NseVal) { .type = TYPE_I64, .cons = (i) })
#define CONS(c) ((NseVal) { .type = TYPE_CONS, .cons = (c) })
#define QUOTE(q) ((NseVal) { .type = TYPE_QUOTE, .quote = (q) })
#define FUNC(f) ((NseVal) { .type = TYPE_FUNC, .func = (f) })

typedef struct NseVal NseVal;
typedef struct cons Cons;
typedef struct closure Closure;
typedef struct quote Quote;

struct NseVal {
  uint8_t type;
  union {
    int64_t i64;
    Cons *cons;
    Quote *quote;
    Quote *type_quote;
    char *symbol;
    NseVal (*func)(NseVal);
    Closure *closure;
  };
};

struct cons {
  size_t refs;
  NseVal *h;
  NseVal *t;
};

struct closure {
  size_t refs;
  NseVal (*f)(NseVal, NseVal[]);
  NseVal env[];
};

struct quote {
  size_t refs;
  NseVal quoted;
};

struct pos {
};

struct syntax {
  size_t refs;
  size_t start_line;
  size_t start_column;
  size_t end_line;
  size_t end_column;
  const char *file;
  NseVal quoted;
};

extern NseVal Undefined;
extern NseVal Nil;
cons *cons(NseVal h, NseVal t);
NseVal QCons(NseVal h, NseVal t);
NseVal Quote(NseVal p);
NseVal QQuote(NseVal p);
NseVal symbol(const char *s);
closure *closure(NseVal f(cons *, NseVal[]), NseVal env[], size_t env_size);

NseVal add_ref(NseVal p);
void del_ref(NseVal p);

Cons *cast_cons(NseVal val);
Closure *cast_closure(NseVal val);
int cast_i64(NseVal in, int64_t *out);

NseVal head(NseVal cons);
NseVal tail(NseVal cons);
size_t list_length(NseVal cons);

int is_symbol(NseVal v, const char *sym);
int is_true(NseVal b);
NseVal nse_apply(NseVal func, NseVal args);
NseVal nse_and(NseVal a, NseVal b);
NseVal nse_equals(NseVal a, NseVal b);

NseVal read(const char *s);
NseVal eval(NseVal value);
NseVal macroexpand(NseVal value);
NseVal macroexpand1(NseVal value);
NseVal print(NseVal value);

#endif
