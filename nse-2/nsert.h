#ifndef NSERT_H
#define NSERT_H

#include <stdlib.h>
#include <stdint.h>

#define TYPE_UNDEFINED '0'
#define TYPE_NIL 'n'
#define TYPE_CONS '.'
#define TYPE_I64 'i'
#define TYPE_SYMBOL 's'
#define TYPE_QUOTE '\''
#define TYPE_TYPE_QUOTE 6
#define TYPE_SYNTAX 'x'
#define TYPE_FUNC 'f'
#define TYPE_CLOSURE 'c'
#define TYPE_REFERENCE 'r'

#define I64(i) ((NseVal) { .type = TYPE_I64, .i64 = (i) })
#define FUNC(f) ((NseVal) { .type = TYPE_FUNC, .func = (f) })

#define CONS(c) ((NseVal) { .type = c ? TYPE_CONS : TYPE_UNDEFINED, .cons = (c) })
#define SYNTAX(c) ((NseVal) { .type = ((c) ? TYPE_SYNTAX : TYPE_UNDEFINED), .syntax = (c) })
#define CLOSURE(c) ((NseVal) { .type = c ? TYPE_CLOSURE : TYPE_UNDEFINED, .closure = (c) })
#define SYMBOL(s) ((NseVal) { .type = s ? TYPE_SYMBOL : TYPE_UNDEFINED, .symbol = (s) })
#define QUOTE(q) ((NseVal) { .type = q ? TYPE_QUOTE : TYPE_UNDEFINED, .quote = (q) })
#define REFERENCE(r) ((NseVal) { .type = r ? TYPE_REFERENCE : TYPE_UNDEFINED, .reference = (r) })

#define TRUE (SYMBOL(create_symbol("t")))
#define FALSE (SYMBOL(create_symbol("f")))

#define RESULT_OK(value) ((value).type != TYPE_UNDEFINED)

typedef struct nse_val NseVal;
typedef struct cons Cons;
typedef struct closure Closure;
typedef struct quote Quote;
typedef struct syntax Syntax;
typedef struct reference Reference;
typedef char Symbol;

typedef void (* Destructor)(void *);

struct nse_val {
  uint8_t type;
  union {
    int64_t i64;
    Cons *cons;
    Syntax *syntax;
    Quote *quote;
    Quote *type_quote;
    char *symbol;
    NseVal (*func)(NseVal);
    Closure *closure;
    Reference *reference;
  };
};

struct cons {
  size_t refs;
  NseVal head;
  NseVal tail;
};

struct closure {
  size_t refs;
  NseVal (*f)(NseVal, NseVal[]);
  size_t env_size;
  NseVal env[];
};

struct quote {
  size_t refs;
  NseVal quoted;
};

struct reference {
  size_t refs;
  void *pointer;
  Destructor destructor;
};

struct syntax {
  size_t refs;
  size_t start_line; size_t start_column; size_t end_line;
  size_t end_column;
  const char *file;
  NseVal quoted;
};

extern NseVal undefined;
extern NseVal nil;
Cons *create_cons(NseVal h, NseVal t);
Quote *create_quote(NseVal quoted);
Syntax *create_syntax(NseVal quoted);
Symbol *create_symbol(const char *s);
Closure *create_closure(NseVal f(NseVal, NseVal[]), NseVal env[], size_t env_size);
Reference *create_reference(void *pointer, void destructor(void *));

char *to_symbol(NseVal v);

extern Syntax *error_form;
extern char *error_string;
void set_debug_form(Syntax *syntax);
Syntax *push_debug_form(Syntax *syntax);
NseVal pop_debug_form(NseVal result, Syntax *previous);
void raise_error(const char *format, ...);

NseVal add_ref(NseVal p);
void del_ref(NseVal p);


NseVal head(NseVal cons);
NseVal tail(NseVal cons);
size_t list_length(NseVal list);

int is_cons(NseVal v);
int is_nil(NseVal v);
int is_list(NseVal v);
int is_i64(NseVal v);
int is_function(NseVal v);
int is_symbol(NseVal v);
int is_true(NseVal b);

int match_symbol(NseVal v, const char *sym);

NseVal nse_apply(NseVal func, NseVal args);
NseVal nse_and(NseVal a, NseVal b);
NseVal nse_equals(NseVal a, NseVal b);

NseVal syntax_to_datum(NseVal v);

NseVal print(NseVal value);
NseVal printr(NseVal value);

#endif
