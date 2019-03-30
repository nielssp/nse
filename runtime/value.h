#ifndef NSE_VALUE_H
#define NSE_VALUE_H

#include <stdlib.h>
#include <stdint.h>

#include "type.h"

#define I64(i) ((NseVal) { .type = i64_type, .i64 = (i) })
#define F64(i) ((NseVal) { .type = f64_type, .f64 = (i) })
#define FUNC(f, arity, variadic) ((NseVal) { .type = get_func_type(arity, variadic), .func = (f) })

#define CONS(c) from_cons(c)
#define SYNTAX(c) ((NseVal) { .type = syntax_type, .syntax = (c) })
#define CLOSURE(c) from_closure(c)
#define SYMBOL(s) ((NseVal) { .type = symbol_type, .symbol = (s) })
#define KEYWORD(s) ((NseVal) { .type = keyword_type, .symbol = (s) })
#define STRING(s) ((NseVal) { .type = string_type, .string = (s) })
#define QUOTE(q) ((NseVal) { .type = quote_type, .quote = (q) })
#define TQUOTE(q) ((NseVal) { .type = type_quote_type, .quote = (q) })
#define CONTINUE(q) ((NseVal) { .type = continue_type, .quote = (q) })
#define TYPE(t) ((NseVal) { .type = type_type, .type_val = (t) })
#define REFERENCE(r) from_reference(r)
#define DATA(d) from_data(d)

#define RESULT_OK(value) ((value).type != NULL)
#define THEN(previous, next) ((RESULT_OK(previous)) ? (next) : undefined)

#define ARG_POP_ANY(name, args) NseVal name = head(args);\
  if (!RESULT_OK(name)) {\
    raise_error(domain_error, "too few parameters for function");\
    return undefined;\
  }\
  args = tail(args);
#define ARG_POP_I64(name, args) int64_t name;\
  {\
    NseVal temp1 = head(args);\
    if (!RESULT_OK(temp1)) {\
      raise_error(domain_error, "too few parameters for function");\
      return undefined;\
    }\
    args = tail(args);\
    if (!is_i64(temp1)) {\
      char *temp2 = nse_write_to_string(temp1, lang_module);\
      raise_error(domain_error, "%s is not an integer", temp2);\
      free(temp2);\
      return undefined;\
    }\
    name = temp1.i64;\
  }
#define ARG_POP_TYPE(type, name, args, convert, type_name) type name;\
  {\
    NseVal temp1 = head(args);\
    if (!RESULT_OK(temp1)) {\
      raise_error(domain_error, "too few parameters for function");\
      return undefined;\
    }\
    args = tail(args);\
    name = convert(temp1);\
    if (name == NULL) {\
      char *temp2 = nse_write_to_string(temp1, lang_module);\
      raise_error(domain_error, "%s is not %s", temp2, type_name);\
      free(temp2);\
      return undefined;\
    }\
  }
#define ARG_POP_REF(ptr_type, name, args, nse_type) ptr_type name;\
  {\
    NseVal temp1 = head(args);\
    if (!RESULT_OK(temp1)) {\
      raise_error(domain_error, "too few parameters for function");\
      return undefined;\
    }\
    args = tail(args);\
    if (temp1.type != nse_type) {\
      char *temp2 = nse_write_to_string(temp1, lang_module);\
      char *temp3 = nse_write_to_string(TYPE(nse_type), lang_module);\
      raise_error(domain_error, "%s is not a %s", temp2, temp3);\
      free(temp2);\
      free(temp3);\
      return undefined;\
    }\
    name = (ptr_type)temp1.reference->pointer;\
  }
#define ARG_DONE(args) if (!is_nil(args)) {\
  raise_error(domain_error, "too many parameters for function");\
  return undefined;\
}

typedef struct NseVal NseVal;
typedef struct Cons Cons;
typedef struct Closure Closure;
typedef struct Quote Quote;
typedef struct Quote TypeQuote;
typedef struct Quote Continue;
typedef struct Syntax Syntax;
typedef struct Reference Reference;
typedef struct String String;
typedef struct Symbol Symbol;
typedef struct Data Data;

typedef void (* Destructor)(void *);

#include "../module.h"
#include "../lang.h"

struct NseVal {
  CType *type;
  union {
    int64_t i64;
    double f64;
    Cons *cons;
    Syntax *syntax;
    Quote *quote;
    Symbol *symbol;
    String *string;
    NseVal (*func)(NseVal);
    CType *type_val;
    Closure *closure;
    Reference *reference;
    Data *data;
  };
};

struct Cons {
  size_t refs;
  CType *type;
  NseVal head;
  NseVal tail;
};

struct Closure {
  size_t refs;
  NseVal (*f)(NseVal, NseVal[]);
  String *doc;
  CType *type;
  size_t env_size;
  NseVal env[];
};

struct Quote {
  size_t refs;
  NseVal quoted;
};

struct String {
  size_t refs;
  size_t length;
  char chars[];
};

struct Reference {
  size_t refs;
  CType *type;
  void *pointer;
  Destructor destructor;
};

struct Syntax {
  size_t refs;
  size_t start_line;
  size_t start_column;
  size_t end_line;
  size_t end_column;
  String *file;
  NseVal quoted;
};

struct Symbol {
  size_t refs;
  Module *module;
  char name[];
};

struct Data {
  size_t refs;
  CType *type;
  Symbol *tag;
  size_t record_size;
  NseVal record[];
};

extern NseVal undefined;
extern NseVal nil;

void init_values();

Cons *create_cons(NseVal h, NseVal t);
Quote *create_quote(NseVal quoted);
TypeQuote *create_type_quote(NseVal quoted);
Continue *create_continue(NseVal args);
Syntax *create_syntax(NseVal quoted);
Symbol *create_symbol(const char *s, Module *module);
Symbol *create_keyword(const char *s, Module *module);
String *create_string(const char *s, size_t length);
Closure *create_closure(NseVal f(NseVal, NseVal[]), CType *type, NseVal env[], size_t env_size);
Reference *create_reference(CType *type, void *pointer, void destructor(void *));
void void_destructor(void * p);
Data *create_data(CType *type, Symbol *tag, NseVal record[], size_t record_size);

Syntax *copy_syntax(Syntax *syntax, NseVal quoted);
NseVal check_alloc(NseVal v);

NseVal from_cons(Cons *c);
NseVal from_closure(Closure *c);
NseVal from_reference(Reference *r);
NseVal from_data(Data *d);

NseVal strip_syntax(NseVal v);

Cons *to_cons(NseVal v);
Symbol *to_symbol(NseVal v);
String *to_string(NseVal v);
Symbol *to_keyword(NseVal v);
const char *to_string_constant(NseVal v);
void *to_reference(NseVal v);
CType *to_type(NseVal v);

extern Syntax *error_form;
void set_debug_form(NseVal form);
Syntax *push_debug_form(Syntax *syntax);
NseVal pop_debug_form(NseVal result, Syntax *previous);
NseVal get_stack_trace();
void clear_stack_trace();

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
int is_f64(NseVal v);
int is_quote(NseVal v);
int is_type_quote(NseVal v);
int is_function(NseVal v);
int is_reference(NseVal v);
int is_symbol(NseVal v);
int is_string(NseVal v);
int is_keyword(NseVal v);
int is_type(NseVal v);
int is_true(NseVal b);

int compare_symbol(NseVal v, const Symbol *sym);
int is_special_form(NseVal v);

NseVal nse_apply(NseVal func, NseVal args);
NseVal nse_and(NseVal a, NseVal b);
NseVal nse_equals(NseVal a, NseVal b);

NseVal syntax_to_datum(NseVal v);

#endif
