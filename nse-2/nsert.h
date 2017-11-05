#ifndef NSERT_H
#define NSERT_H

#include <stdlib.h>
#include <stdint.h>

#include "error.h"
#include "type.h"

#define I64(i) ((NseVal) { .type = TYPE_I64, .i64 = (i) })
#define FUNC(f) ((NseVal) { .type = TYPE_FUNC, .func = (f) })

#define CONS(c) ((NseVal) { .type = TYPE_CONS, .cons = (c) })
#define SYNTAX(c) ((NseVal) { .type = TYPE_SYNTAX, .syntax = (c) })
#define CLOSURE(c) ((NseVal) { .type = TYPE_CLOSURE, .closure = (c) })
#define SYMBOL(s) ((NseVal) { .type = TYPE_SYMBOL, .symbol = (s) })
#define KEYWORD(s) ((NseVal) { .type = TYPE_KEYWORD, .symbol = (s) })
#define STRING(s) ((NseVal) { .type = TYPE_STRING, .string = (s) })
#define QUOTE(q) ((NseVal) { .type = TYPE_QUOTE, .quote = (q) })
#define TQUOTE(q) ((NseVal) { .type = TYPE_TQUOTE, .quote = (q) })
#define TYPE(t) ((NseVal) { .type = TYPE_TYPE, .type_val = (t) })
#define REFERENCE(r) ((NseVal) { .type = TYPE_REFERENCE, .reference = (r) })

#define RESULT_OK(value) ((value).type != TYPE_UNDEFINED)

#define ARG_POP_ANY(name, args) NseVal name = head(args);\
  if (!RESULT_OK(name)) {\
    raise_error("too few parameters for function");\
    return undefined;\
  }\
  args = tail(args);
#define ARG_POP_TYPE(type, name, args, convert, type_name) type name;\
  {\
    NseVal temp1 = head(args);\
    if (!RESULT_OK(temp1)) {\
      raise_error("too few parameters for function");\
      return undefined;\
    }\
    args = tail(args);\
    name = convert(temp1);\
    if (name == NULL) {\
      char *temp2 = nse_write_to_string(temp1, lang_module);\
      raise_error("%s is not %s", temp2, type_name);\
      free(temp2);\
      return undefined;\
    }\
  }
#define ARG_DONE(args) if (!is_nil(args)) {\
  raise_error("too many parameters for function");\
  return undefined;\
}

typedef enum {
 TYPE_UNDEFINED,
 TYPE_NIL,
 TYPE_CONS,
 TYPE_I64,
 TYPE_SYMBOL,
 TYPE_KEYWORD,
 TYPE_STRING,
 TYPE_QUOTE,
 TYPE_TQUOTE,
 TYPE_SYNTAX,
 TYPE_FUNC,
 TYPE_CLOSURE,
 TYPE_REFERENCE,
 TYPE_TYPE
} NseValType;

typedef struct nse_val NseVal;
typedef struct cons Cons;
typedef struct closure Closure;
typedef struct quote Quote;
typedef struct quote TypeQuote;
typedef struct syntax Syntax;
typedef struct reference Reference;
typedef struct string String;
typedef struct symbol Symbol;

typedef void (* Destructor)(void *);

struct nse_val {
  NseValType type;
  union {
    int64_t i64;
    Cons *cons;
    Syntax *syntax;
    Quote *quote;
    Quote *type_quote;
    Symbol *symbol;
    String *string;
    NseVal (*func)(NseVal);
    Type *type_val;
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
  Type *type;
  size_t env_size;
  NseVal env[];
};

struct quote {
  size_t refs;
  NseVal quoted;
};

struct string {
  size_t refs;
  size_t length;
  char chars[];
};

struct reference {
  size_t refs;
  void *pointer;
  Destructor destructor;
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

#include "module.h"

struct symbol {
  size_t refs;
  Module *module;
  char name[];
};

#include "lang.h"

extern NseVal undefined;
extern NseVal nil;
Cons *create_cons(NseVal h, NseVal t);
Quote *create_quote(NseVal quoted);
TypeQuote *create_type_quote(NseVal quoted);
Syntax *create_syntax(NseVal quoted);
Symbol *create_symbol(const char *s, Module *module);
Symbol *create_keyword(const char *s, Module *module);
String *create_string(const char *s, size_t length);
Closure *create_closure(NseVal f(NseVal, NseVal[]), Type *type, NseVal env[], size_t env_size);
Reference *create_reference(void *pointer, void destructor(void *));

Syntax *copy_syntax(Syntax *syntax, NseVal quoted);
NseVal check_alloc(NseVal v);

Symbol *to_symbol(NseVal v);
Symbol *to_keyword(NseVal v);
void *to_reference(NseVal v);
Type *to_type(NseVal v);

extern Syntax *error_form;
extern char *error_string;
void set_debug_form(Syntax *syntax);
Syntax *push_debug_form(Syntax *syntax);
NseVal pop_debug_form(NseVal result, Syntax *previous);
void raise_error(const char *format, ...);
void clear_error();

NseVal add_ref(NseVal p);
void del_ref(NseVal p);


NseVal head(NseVal cons);
NseVal tail(NseVal cons);
NseVal elem(size_t n, NseVal cons);
size_t list_length(NseVal list);

int is_cons(NseVal v);
int is_nil(NseVal v);
int is_list(NseVal v);
int is_i64(NseVal v);
int is_quote(NseVal v);
int is_type_quote(NseVal v);
int is_function(NseVal v);
int is_reference(NseVal v);
int is_symbol(NseVal v);
int is_keyword(NseVal v);
int is_type(NseVal v);
int is_true(NseVal b);

int match_symbol(NseVal v, const Symbol *sym);
int is_special_form(NseVal v);

NseVal nse_apply(NseVal func, NseVal args);
NseVal nse_and(NseVal a, NseVal b);
NseVal nse_equals(NseVal a, NseVal b);

NseVal syntax_to_datum(NseVal v);

const char *nse_val_type_to_string(NseValType t);

Type *get_type(NseVal v);

#endif
