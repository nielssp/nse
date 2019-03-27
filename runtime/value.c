#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "hashmap.h"
#include "error.h"

#include "value.h"

static void delete_all(NseVal value);
static void delete(NseVal value);

NseVal undefined = { .type = NULL };
NseVal nil;

Syntax *error_form = NULL;

NseVal stack_trace;

void init_values() {
  init_types();
  nil = (NseVal){ .type = nil_type };
  stack_trace = nil;
}

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

Cons *create_cons(NseVal h, NseVal t) {
  Cons *cons = allocate(sizeof(Cons));
  if (!cons) {
    return NULL;
  }
  cons->refs = 1;
  if (t.type == nil_type) {
    cons->type = get_unary_instance(list_type, copy_type(h.type));
  } else if (t.type->type == C_TYPE_INSTANCE && t.type->instance.type == list_type) {
    CType *existing = t.type->instance.parameters[0];
    if (existing == h.type) {
      cons->type = copy_type(t.type);
    } else {
      cons->type = get_unary_instance(list_type, unify_types(h.type, existing));
    }
  } else {
    cons->type = copy_type(improper_list_type);
  }
  cons->head = h;
  cons->tail = t;
  add_ref(h);
  add_ref(t);
  return cons;
}

Quote *create_quote(NseVal quoted) {
  Quote *quote = allocate(sizeof(Quote));
  if (!quote) {
    return NULL;
  }
  quote->refs = 1;
  quote->quoted = quoted;
  add_ref(quoted);
  return quote;
}

TypeQuote *create_type_quote(NseVal quoted) {
  return create_quote(quoted);
}

Continue *create_continue(NseVal args) {
  return create_quote(args);
}

Syntax *create_syntax(NseVal quoted) {
  Syntax *syntax = allocate(sizeof(Syntax));
  if (!syntax) {
    return NULL;
  }
  syntax->refs = 1;
  syntax->start_line = 0;
  syntax->start_column = 0;
  syntax->end_line = 0;
  syntax->end_column = 0;
  syntax->file = NULL;
  syntax->quoted = quoted;
  add_ref(quoted);
  return syntax;
}

Symbol *create_symbol(const char *s, Module *module) {
  size_t len = strlen(s);
  Symbol *symbol = allocate(sizeof(Symbol) + len + 1);
  if (!symbol) {
    return NULL;
  }
  symbol->refs = 1;
  symbol->module = module;
  memcpy(symbol->name, s, len);
  symbol->name[len] = '\0';
  return symbol;
}

Symbol *create_keyword(const char *s, Module *module) {
  return create_symbol(s, module);
}


String *create_string(const char *s, size_t length) {
  String *str = allocate(sizeof(String) + length + 1);
  if (!str) {
    return NULL;
  }
  str->refs = 1;
  str->length = length;
  memcpy(str->chars, s, length);
  str->chars[length] = '\0';
  return str;
}

Closure *create_closure(NseVal f(NseVal, NseVal[]), CType *type, NseVal env[], size_t env_size) {
  Closure *closure = allocate(sizeof(Closure) + env_size * sizeof(NseVal));
  if (!closure) {
    delete_type(type);
    return NULL;
  }
  closure->refs = 1;
  closure->f = f;
  closure->type = type;
  closure->doc = NULL;
  closure->env_size = env_size;
  if (env_size > 0) {
    memcpy(closure->env, env, env_size * sizeof(NseVal));
    for (size_t i = 0; i < env_size; i++) {
      add_ref(env[i]);
    }
  }
  return closure;
}

Reference *create_reference(CType *type, void *pointer, void destructor(void *)) {
  Reference *reference = allocate(sizeof(Reference));
  if (!reference) {
    delete_type(type);
    return NULL;
  }
  reference->refs = 1;
  reference->type = type;
  reference->pointer = pointer;
  reference->destructor = destructor;
  return reference;
}

void void_destructor(void * p) {
}

Data *create_data(CType *type, Symbol *tag, NseVal record[], size_t record_size) {
  Data *data = allocate(sizeof(Data) + record_size * sizeof(NseVal));
  if (!data) {
    delete_type(type);
    return NULL;
  }
  data->refs = 1;
  data->type = type;
  add_ref(SYMBOL(tag));
  data->tag = tag;
  data->record_size = record_size;
  if (record_size > 0) {
    memcpy(data->record, record, record_size * sizeof(NseVal));
    for (size_t i = 0; i < record_size; i++) {
      add_ref(record[i]);
    }
  }
  return data;
}

Syntax *copy_syntax(Syntax *syntax, NseVal quoted) {
  Syntax *copy = create_syntax(quoted);
  if (copy) {
    copy->start_line = syntax->start_line;
    copy->start_column = syntax->start_column;
    copy->end_line = syntax->end_line;
    copy->end_column = syntax->end_column;
    copy->file = syntax->file;
    if (copy->file) {
      add_ref(STRING(copy->file));
    }
  }
  return copy;
}

NseVal check_alloc(NseVal v) {
  if (!v.type) {
    return undefined;
  }
  switch (v.type->internal) {
    case INTERNAL_CONS:
    case INTERNAL_CLOSURE:
    case INTERNAL_QUOTE:
    case INTERNAL_SYNTAX:
    case INTERNAL_REFERENCE:
    case INTERNAL_SYMBOL:
    case INTERNAL_STRING:
    case INTERNAL_TYPE:
    case INTERNAL_DATA:
      if ((void *)v.cons == NULL) {
        return undefined;
      }
    default:
      return v;
  }
}

NseVal add_ref(NseVal value) {
  if (!value.type) {
    return value;
  }
  switch (value.type->internal) {
    case INTERNAL_CONS:
      value.cons->refs++;
      break;
    case INTERNAL_CLOSURE:
      value.closure->refs++;
      break;
    case INTERNAL_QUOTE:
      value.quote->refs++;
      break;
    case INTERNAL_SYMBOL:
      value.symbol->refs++;
      break;
    case INTERNAL_STRING:
      value.string->refs++;
      break;
    case INTERNAL_SYNTAX:
      value.syntax->refs++;
      break;
    case INTERNAL_REFERENCE:
      value.reference->refs++;
      break;
    case INTERNAL_TYPE:
      copy_type(value.type_val);
      break;
    case INTERNAL_DATA:
      value.data->refs++;
      break;
    default:
      break;
  }
  return value;
}

void del_ref(NseVal value) {
  if (!value.type) {
    return;
  }
  size_t *refs = NULL;
  switch (value.type->internal) {
    case INTERNAL_CONS:
      refs = &value.cons->refs;
      break;
    case INTERNAL_CLOSURE:
      refs = &value.closure->refs;
      break;
    case INTERNAL_QUOTE:
      refs = &value.quote->refs;
      break;
    case INTERNAL_SYMBOL:
      refs = &value.symbol->refs;
      break;
    case INTERNAL_STRING:
      refs = &value.string->refs;
      break;
    case INTERNAL_SYNTAX:
      refs = &value.syntax->refs;
      break;
    case INTERNAL_REFERENCE:
      refs = &value.reference->refs;
      break;
    case INTERNAL_TYPE:
      delete_type(value.type_val);
      return;
    case INTERNAL_DATA:
      refs = &value.data->refs;
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

static void delete_all(NseVal value) {
  if (value.type->internal == INTERNAL_CONS) {
    del_ref(value.cons->head);
    del_ref(value.cons->tail);
  }
  if (value.type->internal == INTERNAL_QUOTE) {
    del_ref(value.quote->quoted);
  }
  if (value.type->internal == INTERNAL_CLOSURE) {
    if (value.closure->doc) {
      del_ref(STRING(value.closure->doc));
    }
    delete_type(value.closure->type);
    for (size_t i = 0; i < value.closure->env_size; i++) {
      del_ref(value.closure->env[i]);
    }
  }
  if (value.type->internal == INTERNAL_SYNTAX) {
    if (value.syntax->file) {
      del_ref(STRING(value.syntax->file));
    }
    del_ref(value.syntax->quoted);
  }
  if (value.type->internal == INTERNAL_DATA) {
    del_ref(SYMBOL(value.data->tag));
    for (size_t i = 0; i < value.data->record_size; i++) {
      del_ref(value.data->record[i]);
    }
  }
  delete(value);
}

static void delete(NseVal value) {
  switch (value.type->internal) {
    case INTERNAL_CONS:
      free(value.cons);
      return;
    case INTERNAL_SYMBOL:
      free(value.symbol);
      return;
    case INTERNAL_QUOTE:
      free(value.quote);
      return;
    case INTERNAL_STRING:
      free(value.string);
      return;
    case INTERNAL_SYNTAX:
      free(value.syntax);
      return;
    case INTERNAL_CLOSURE:
      free(value.closure);
      return;
    case INTERNAL_REFERENCE:
      if (value.reference->destructor) {
        value.reference->destructor(value.reference->pointer);
      }
      delete_type(value.reference->type);
      free(value.reference);
      return;
    case INTERNAL_DATA:
      delete_type(value.data->type);
      free(value.data);
      return;
    default:
      return;
  }
}

NseVal head(NseVal value) {
  NseVal result = undefined;
  if (value.type->internal == INTERNAL_CONS) {
    result = value.cons->head;
  } else if (value.type->internal == INTERNAL_SYNTAX) {
    return head(value.syntax->quoted);
  } else {
    raise_error(domain_error, "head of empty list");
  }
  return result;
}

NseVal tail(NseVal value) {
  NseVal result = undefined;
  if (value.type->internal == INTERNAL_CONS) {
    result = value.cons->tail;
  } else if (value.type->internal == INTERNAL_SYNTAX) {
    return tail(value.syntax->quoted);
  } else {
    raise_error(domain_error, "tail of empty list");
  }
  return result;
}

NseVal elem(size_t n, NseVal value) {
  for (size_t i = 0; i < n; i++) {
    value = tail(value);
    if (!RESULT_OK(value)) {
      return undefined;
    }
  }
  return head(value);
}

int is_cons(NseVal v) {
  if (v.type->internal == INTERNAL_CONS) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_cons(v.syntax->quoted);
  }
  return 0;
}

int is_nil(NseVal v) {
  if (v.type->internal == INTERNAL_NIL) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_nil(v.syntax->quoted);
  }
  return 0;
}

int is_list(NseVal v) {
  if (v.type->internal == INTERNAL_CONS || v.type->internal == INTERNAL_NIL) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_list(v.syntax->quoted);
  }
  return 0;
}

int is_i64(NseVal v) {
  if (v.type->internal == INTERNAL_I64) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_i64(v.syntax->quoted);
  }
  return 0;
}

int is_f64(NseVal v) {
  if (v.type->internal == INTERNAL_F64) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_f64(v.syntax->quoted);
  }
  return 0;
}

int is_function(NseVal v) {
  if (v.type->internal == INTERNAL_FUNC || v.type->internal == INTERNAL_CLOSURE) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_function(v.syntax->quoted);
  }
  return 0;
}

int is_reference(NseVal v) {
  if (v.type->internal == INTERNAL_REFERENCE) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_reference(v.syntax->quoted);
  }
  return 0;
}

int is_quote(NseVal v) {
  if (v.type->internal == INTERNAL_QUOTE) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_quote(v.syntax->quoted);
  }
  return 0;
}

int is_type_quote(NseVal v) {
  if (v.type == type_quote_type) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_type_quote(v.syntax->quoted);
  }
  return 0;
}

int is_string(NseVal v) {
  if (v.type->internal == INTERNAL_STRING) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_string(v.syntax->quoted);
  }
  return 0;
}

int is_symbol(NseVal v) {
  if (v.type->internal == INTERNAL_SYMBOL) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_symbol(v.syntax->quoted);
  }
  return 0;
}

int is_type(NseVal v) {
  if (v.type->internal == INTERNAL_TYPE) {
    return 1;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return is_type(v.syntax->quoted);
  }
  return 0;
}

NseVal from_cons(Cons *c) {
  return (NseVal){ .type = c->type, .cons = c };
}

NseVal from_closure(Closure *c) {
  return (NseVal){ .type = c->type, .closure = c };
}

NseVal from_reference(Reference *r) {
  return (NseVal){ .type = r->type, .reference = r };
}

NseVal from_data(Data *d) {
  return (NseVal){ .type = d->type, .data = d };
}

Cons *to_cons(NseVal v) {
  if (v.type->internal == INTERNAL_CONS) {
    return v.cons;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return to_cons(v.syntax->quoted);
  }
  return NULL;
}

Symbol *to_symbol(NseVal v) {
  if (v.type == symbol_type) {
    return v.symbol;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return to_symbol(v.syntax->quoted);
  }
  return NULL;
}

String *to_string(NseVal v) {
  if (v.type->internal == INTERNAL_STRING) {
    return v.string;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return to_string(v.syntax->quoted);
  }
  return NULL;
}

Symbol *to_keyword(NseVal v) {
  if (v.type == keyword_type) {
    return v.symbol;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return to_keyword(v.syntax->quoted);
  }
  return NULL;
}

const char *to_string_constant(NseVal v) {
  if (v.type->internal == INTERNAL_SYMBOL) {
    return v.symbol->name;
  } else if (v.type->internal == INTERNAL_STRING) {
    return v.string->chars;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return to_string_constant(v.syntax->quoted);
  }
  return NULL;
}

void *to_reference(NseVal v) {
  if (v.type->internal == INTERNAL_REFERENCE) {
    return v.reference->pointer;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return to_reference(v.syntax->quoted);
  }
  return NULL;
}

CType *to_type(NseVal v) {
  if (v.type->internal == INTERNAL_TYPE) {
    return v.type_val;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    return to_type(v.syntax->quoted);
  }
  return NULL;
}

int match_symbol(NseVal v, const Symbol *symbol) {
  int result = 0;
  if (v.type == symbol_type) {
    result = v.symbol == symbol;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    result = match_symbol(v.syntax->quoted, symbol);
  }
  return result;
}

int is_special_form(NseVal v) {
  int result = 0;
  if (v.type == symbol_type) {
    result |= v.symbol == if_symbol;
    result |= v.symbol == fn_symbol;
    result |= v.symbol == let_symbol;
    result |= v.symbol == try_symbol;
    result |= v.symbol == def_symbol;
    result |= v.symbol == def_macro_symbol;
    result |= v.symbol == def_type_symbol;
    result |= v.symbol == def_data_symbol;
  } else if (v.type->internal == INTERNAL_SYNTAX) {
    result = is_special_form(v.syntax->quoted);
  }
  return result;
}

int is_true(NseVal b) {
  return match_symbol(b, t_symbol);
}

size_t list_length(NseVal value) {
  size_t count = 0;
  while (value.type->internal == INTERNAL_CONS) {
    count++;
    value = value.cons->tail;
  }
  return count;
}

static int stack_trace_push(NseVal func, NseVal args) {
  if (!error_form) {
    return 1;
  }
  Cons *c1 = create_cons(SYNTAX(error_form), nil);
  if (!c1) {
    return 0;
  }
  Cons *c2 = create_cons(args, CONS(c1));
  del_ref(CONS(c1));
  if (!c2) {
    return 0;
  }
  Cons *c3 = create_cons(func, CONS(c2));
  del_ref(CONS(c2));
  if (!c3) {
    return 0;
  }
  NseVal old = stack_trace;
  Cons *new_trace = create_cons(CONS(c3), old);
  del_ref(CONS(c3));
  if (!new_trace) {
    return 0;
  }
  stack_trace = CONS(new_trace);
  del_ref(old);
  return 1;
}

static void stack_trace_pop() {
  NseVal current = stack_trace;
  stack_trace = add_ref(tail(current));
  del_ref(current);
}

NseVal get_stack_trace() {
  return add_ref(stack_trace);
}

void clear_stack_trace() {
  del_ref(stack_trace);
  stack_trace = nil;
}

NseVal nse_apply(NseVal func, NseVal args) {
  NseVal result = undefined;
  if (func.type->internal == INTERNAL_FUNC) {
    if (!stack_trace_push(func, args)) {
      return undefined;
    }
    result = func.func(args);
  } else if (func.type->internal == INTERNAL_CLOSURE) {
    if (!stack_trace_push(func, args)) {
      return undefined;
    }
    result = func.closure->f(args, func.closure->env);
  } else {
    raise_error(domain_error, "not a function");
  }
  if (RESULT_OK(result)) {
    stack_trace_pop();
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
  if (a.type == NULL || b.type == NULL) {
    return undefined;
  }
  if (a.type->internal == INTERNAL_SYNTAX) {
    return nse_equals(a.syntax->quoted, b);
  }
  if (b.type->internal == INTERNAL_SYNTAX) {
    return nse_equals(a, b.syntax->quoted);
  }
  if (a.type != b.type) {
    return FALSE;
  }
  switch (a.type->internal) {
    case INTERNAL_NIL:
      return TRUE;
    case INTERNAL_CONS:
      return nse_and(nse_equals(head(a), head(b)), nse_equals(tail(a), tail(b)));
    case INTERNAL_STRING:
      if (a.string->length != b.string->length) {
        return FALSE;
      }
      return strncmp(a.string->chars, b.string->chars, a.string->length) == 0 ? TRUE : FALSE;
    case INTERNAL_SYMBOL:
      if (a.symbol == b.symbol) {
        return TRUE;
      }
      return FALSE;
    case INTERNAL_QUOTE:
      return nse_equals(a.quote->quoted, b.quote->quoted);
    case INTERNAL_I64:
      if (a.i64 == b.i64) {
        return TRUE;
      }
      return FALSE;
    case INTERNAL_TYPE:
      return a.type_val == b.type_val ? TRUE : FALSE;
    case INTERNAL_DATA:
      if (a.data == b.data) {
        return TRUE;
      }
      if (a.data->type != b.data->type) {
        return FALSE;
      }
      if (a.data->tag != b.data->tag) {
        return FALSE;
      }
      if (a.data->record_size != b.data->record_size) {
        return FALSE;
      }
      for (int i = 0; i < a.data->record_size; i++) {
        if (!is_true(nse_equals(a.data->record[i], b.data->record[i]))) {
          return FALSE;
        }
      }
      return TRUE;
    default:
      return FALSE;
  }
}

NseVal syntax_to_datum(NseVal v) {
  switch (v.type->internal) {
    case  INTERNAL_SYNTAX:
      return syntax_to_datum(v.syntax->quoted);
    case  INTERNAL_CONS: {
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
    case INTERNAL_QUOTE: {
      NseVal quote = undefined;
      NseVal quoted = syntax_to_datum(v.quote->quoted);
      if (RESULT_OK(quoted)) {
        quote = check_alloc(QUOTE(create_quote(quoted)));
        quote.type = copy_type(v.type);
        del_ref(quoted);
      }
      return quote;
    }
    default:
      return add_ref(v);
  }
}
