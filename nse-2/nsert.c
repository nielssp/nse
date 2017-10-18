#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "nsert.h"
#include "util/hash_map.h"

DEFINE_PRIVATE_HASH_MAP(symmap, SymMap, char *, char *, string_hash, string_equals)

SymMap symbols;

void delete_all(NseVal value);
void delete(NseVal value);
NseVal parse_list(char **input);

char *error_string = NULL;
Syntax *error_form = NULL;

void set_debug_form(Syntax *syntax) {
  if (error_form) {
    del_ref(SYNTAX(error_form));
  }
  error_form = syntax;
  if (error_form) {
    add_ref(SYNTAX(error_form));
  }
}

Syntax *push_debug_form(Syntax *syntax) {
  Syntax *previous = error_form;
  error_form = syntax;
  if (error_form) {
    add_ref(SYNTAX(error_form));
  }
  return previous;
}

NseVal pop_debug_form(NseVal result, Syntax *previous) {
  if (!RESULT_OK(result)) {
    if (previous) {
      del_ref(SYNTAX(previous));
    }
    return result;
  }
  if (error_form) {
    del_ref(SYNTAX(error_form));
  }
  error_form = previous;
  return result;
}

void raise_error(const char *format, ...) {
  va_list va;
  char *old = error_string;
  char *buffer = malloc(50);
  size_t size = 50;
  while (1) {
    va_start(va, format);
    int n = vsnprintf(buffer, size, format, va);
    va_end(va);
    if (n < 0) {
      // ERROR
      break;
    }
    if (n  < size) {
      error_string = buffer;
      break;
    }
    size_t new_size = n + 1;
    char *new_buffer = malloc(new_size);
    if (!new_buffer) {
      // ERROR
      break;
    }
    memcpy(new_buffer, buffer, size);
    free(buffer);
    buffer = new_buffer;
    size = new_size;
  }
  if (old) {
    free(old);
  }
}

void clear_error() {
  if (error_string) {
    free(error_string);
    error_string = NULL;
  }
}


NseVal undefined = { .type = TYPE_UNDEFINED };
NseVal nil = { .type = TYPE_NIL };

Cons *create_cons(NseVal h, NseVal t) {
  Cons *cons = malloc(sizeof(Cons));
  if (!cons) {
    raise_error("cons: could not allocate %zd bytes of memory", sizeof(Cons));
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
    raise_error("quote: could not allocate %zd bytes of memory", sizeof(Quote));
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
    raise_error("syntax: could not allocate %zd bytes of memory", sizeof(Syntax));
    return NULL;
  }
  syntax->refs = 1;
  syntax->quoted = quoted;
  add_ref(quoted);
  return syntax;
}

Symbol *create_symbol(const char *s) {
  if (symbols.map == NULL) {
    symbols = create_symmap();
    if (!symbols.map) {
      raise_error("symbol: could not allocate symbol map");
      return NULL;
    }
  } else {
    Symbol *value = symmap_lookup(symbols, s);
    if (value != NULL) {
      return value;
    }
  }
  size_t len = strlen(s);
  char *copy = malloc(len + 1);
  if (!copy) {
    raise_error("symbol: could not allocate %zd bytes of memory", len + 1);
    return NULL;
  }
  memcpy(copy, s, len);
  copy[len] = '\0';
  symmap_add(symbols, copy, copy);
  return copy;
}

Closure *create_closure(NseVal f(NseVal, NseVal[]), NseVal env[], size_t env_size) {
  Closure *closure = malloc(sizeof(Closure) + env_size * sizeof(NseVal));
  if (!closure) {
    raise_error("closure: could not allocate %zd bytes of memory", sizeof(Closure) + env_size * sizeof(NseVal));
  }
  closure->refs = 1;
  closure->f = f;
  closure->env_size = env_size;
  if (env_size > 0) {
    memcpy(closure->env, env, env_size * sizeof(NseVal));
    for (size_t i = 0; i < env_size; i++) {
      add_ref(env[i]);
    }
  }
  return closure;
}

Reference *create_reference(void *pointer, void destructor(void *)) {
  Reference *reference = malloc(sizeof(Reference));
  if (!reference) {
    raise_error("reference: could not allocate %zd bytes of memory", sizeof(Reference));
  }
  reference->refs = 1;
  reference->pointer = pointer;
  reference->destructor = destructor;
  return reference;
}

NseVal check_alloc(NseVal v) {
  switch (v.type) {
    case TYPE_CONS:
    case TYPE_CLOSURE:
    case TYPE_QUOTE:
    case TYPE_SYNTAX:
    case TYPE_REFERENCE:
    case TYPE_SYMBOL:
      if ((void *)v.cons == NULL) {
        return undefined;
      }
    default:
      return v;
  }
}

NseVal add_ref(NseVal value) {
  switch (value.type) {
    case TYPE_CONS:
      value.cons->refs++;
      break;
    case TYPE_CLOSURE:
      value.closure->refs++;
      break;
    case TYPE_QUOTE:
      value.quote->refs++;
      break;
    case TYPE_SYNTAX:
      value.syntax->refs++;
      break;
    case TYPE_REFERENCE:
      value.reference->refs++;
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
    case  TYPE_REFERENCE:
      refs = &value.reference->refs;
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
  if (value.type == TYPE_CLOSURE) {
    for (size_t i = 0; i < value.closure->env_size; i++) {
      del_ref(value.closure->env[i]);
    }
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
    case TYPE_REFERENCE:
      value.reference->destructor(value.reference->pointer);
      free(value.reference);
      return;
    default:
      return;
  }
}

NseVal head(NseVal value) {
  NseVal result = undefined;
  if (value.type == TYPE_CONS) {
    result = value.cons->head;
  } else if (value.type == TYPE_SYNTAX) {
    return head(value.syntax->quoted);
  } else {
    raise_error("head of empty list");
  }
  return result;
}

NseVal tail(NseVal value) {
  NseVal result = undefined;
  if (value.type == TYPE_CONS) {
    result = value.cons->tail;
  } else if (value.type == TYPE_SYNTAX) {
    return tail(value.syntax->quoted);
  } else {
    raise_error("tail of empty list");
  }
  return result;
}

int is_cons(NseVal v) {
  if (v.type == TYPE_CONS) {
    return 1;
  } else if (v.type == TYPE_SYNTAX) {
    return is_cons(v.syntax->quoted);
  }
  return 0;
}

int is_nil(NseVal v) {
  if (v.type == TYPE_NIL) {
    return 1;
  } else if (v.type == TYPE_SYNTAX) {
    return is_nil(v.syntax->quoted);
  }
  return 0;
}

int is_list(NseVal v) {
  if (v.type == TYPE_CONS || v.type == TYPE_NIL) {
    return 1;
  } else if (v.type == TYPE_SYNTAX) {
    return is_list(v.syntax->quoted);
  }
  return 0;
}

int is_i64(NseVal v) {
  if (v.type == TYPE_I64) {
    return 1;
  } else if (v.type == TYPE_SYNTAX) {
    return is_i64(v.syntax->quoted);
  }
  return 0;
}

int is_function(NseVal v) {
  if (v.type == TYPE_FUNC || v.type == TYPE_CLOSURE) {
    return 1;
  } else if (v.type == TYPE_SYNTAX) {
    return is_function(v.syntax->quoted);
  }
  return 0;
}

int is_symbol(NseVal v) {
  if (v.type == TYPE_SYMBOL) {
    return 1;
  } else if (v.type == TYPE_SYNTAX) {
    return is_symbol(v.syntax->quoted);
  }
  return 0;
}

char *to_symbol(NseVal v) {
  if (v.type == TYPE_SYMBOL) {
    return v.symbol;
  } else if (v.type == TYPE_SYNTAX) {
    return to_symbol(v.syntax->quoted);
  }
  return NULL;
}

int match_symbol(NseVal v, const char *sym) {
  int result = 0;
  if (v.type == TYPE_SYMBOL) {
    result = strcmp(v.symbol, sym) == 0;
  } else if (v.type == TYPE_SYNTAX) {
    result = match_symbol(v.syntax->quoted, sym);
  }
  return result;
}

int is_true(NseVal b) {
  return match_symbol(b, "t");
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
  } else {
    raise_error("not a function");
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

NseVal syntax_to_datum(NseVal v) {
  switch (v.type) {
    case  TYPE_SYNTAX:
      return syntax_to_datum(v.syntax->quoted);
    case  TYPE_CONS: {
      NseVal cons = undefined;
      NseVal head = syntax_to_datum(v.cons->head);
      if (RESULT_OK(head)) {
        NseVal tail = syntax_to_datum(v.cons->tail);
        if (RESULT_OK(tail)) {
          cons = check_alloc(CONS(create_cons(head, tail)));
          del_ref(tail);
        }
        del_ref(head);
      }
      return cons;
    }
    case TYPE_QUOTE: {
      NseVal quote = undefined;
      NseVal quoted = syntax_to_datum(v.quote->quoted);
      if (RESULT_OK(quoted)) {
        quote = check_alloc(QUOTE(create_quote(quoted)));
        del_ref(quoted);
      }
      return quote;
    }
    case  TYPE_CLOSURE:
    case  TYPE_REFERENCE:
    default:
      return v;
  }
}

void print_cons(Cons *cons);

void print_cons_tail(NseVal tail) {
  if (tail.type == TYPE_SYNTAX) {
    print_cons_tail(tail.syntax->quoted);
  } else if (tail.type == TYPE_CONS) {
    printf(" ");
    print_cons(tail.cons);
  } else if (tail.type != TYPE_NIL) {
    printf(" . ");
    print(tail);
  }
}

void print_cons(Cons *cons) {
  print(cons->head);
  print_cons_tail(cons->tail);
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
    case TYPE_SYNTAX:
      printf("#<syntax ");
      print(value.syntax->quoted);
      printf(">");
      break;
    case TYPE_FUNC:
      printf("#<function>");
      break;
    case TYPE_CLOSURE:
      printf("#<lambda>");
      break;
    default:
      raise_error("undefined type: %d", value.type);
      return undefined;
  }
  return nil;
}

void printr_cons(Cons *cons);

void printr_cons_tail(NseVal tail) {
  if (tail.type == TYPE_SYNTAX) {
    printr_cons_tail(tail.syntax->quoted);
  } else if (tail.type == TYPE_CONS) {
    printf(" ");
    printr_cons(tail.cons);
  } else if (tail.type != TYPE_NIL) {
    printf(" . ");
    printr(tail);
  }
}

void printr_cons(Cons *cons) {
  printr(cons->head);
  printr_cons_tail(cons->tail);
}

NseVal printr(NseVal value) {
  switch (value.type) {
    case TYPE_NIL:
      printf("()");
      break;
    case TYPE_CONS:
      printf("(");
      printr_cons(value.cons);
      printf(")[%zd]", value.cons->refs);
      break;
    case TYPE_SYMBOL:
      printf("%s", value.symbol);
      break;
    case TYPE_I64:
      printf("%ld", value.i64);
      break;
    case TYPE_QUOTE:
      printf("'[%zd]", value.quote->refs);
      printr(value.quote->quoted);
      break;
    case TYPE_SYNTAX:
      printf("#'[%zd]", value.syntax->refs);
      printr(value.syntax->quoted);
      break;
    case TYPE_FUNC:
      printf("#<function>");
      break;
    case TYPE_CLOSURE:
      printf("#<lambda>[%zd]", value.closure->refs);
      break;
    default:
      raise_error("undefined type: %d", value.type);
      return undefined;
  }
  return nil;
}
