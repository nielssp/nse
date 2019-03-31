#include "error.h"

#include "validate.h"

static int validate(NseVal value, Validator validator) {
  while (value.type == syntax_type) {
    value = value.syntax->quoted;
  }
  switch (validator.type) {
    case VALIDATOR_EXACT: {
      Symbol *symbol = to_symbol(value);
      if (!symbol || symbol != validator.exact) {
        return 0;
      }
      break;
    }
    case VALIDATOR_SYMBOL: {
      Symbol *symbol = to_symbol(value);
      if (!symbol) {
        return 0;
      }
      *validator.symbol = symbol;
      break;
    }
    case VALIDATOR_TQUOTE: {
      if (!is_type_quote(value)) {
        return 0;
      }
      *validator.tquote = value.quote;
      break;
    }
    case VALIDATOR_ANY:
      *validator.any = value;
      break;
    case VALIDATOR_LIST:
      if (!validate_list(value, validator.list)) {
        return 0;
      }
      break;
    case VALIDATOR_ALT:
      for (Validator *alt = validator.alt; alt->type != VALIDATOR_END; alt++) {
        if (validate(value, *alt)) {
          return 1;
        }
      }
      return 0;
    default:
      break;
  }
  return 1;
}

int validate_list(NseVal value, Validator validators[]) {
  while (validators->type != VALIDATOR_END) {
    if (validators->type == VALIDATOR_REP) {
      while (is_cons(value) && validate(head(value), *validators->rep)) {
        value = tail(value);
      }
    } else {
      if (!is_cons(value)) {
        return 0;
      }
      if (!validate(head(value), *validators)) {
        return 0;
      }
      value = tail(value);
    }
    validators++;
  }
  return is_nil(value);
}

NseVal *list_to_array(NseVal list, size_t *length) {
  Cons *c = to_cons(list);
  if (!c) {
    set_debug_form(list);
    raise_error(domain_error, "expected a list");
    return NULL;
  }
  *length = 0;
  size_t size = 8;
  NseVal *buffer = allocate(sizeof(NseVal) * size);
  if (!buffer) {
    return buffer;
  }
  NseVal elem;
  while (accept_elem_any(&list, &elem)) {
    if (*length >= size) {
      size *= 2;
      NseVal *new_buffer = realloc(buffer, sizeof(NseVal) * size);
      if (new_buffer) {
        buffer = new_buffer;
      } else {
        free(buffer);
        raise_error(out_of_memory_error, "out of memory");
        return NULL;
      }
    }
    buffer[(*length)++] = elem;
  }
  if (!is_nil(list)) {
    free(buffer);
    set_debug_form(list);
    raise_error(domain_error, "not a proper list");
    return NULL;
  }
  return buffer;
}

#define DEF_ACCEPT_ELEM(NAME, RETURN_TYPE, CONVERTER) \
  int accept_elem_ ## NAME (NseVal *next, RETURN_TYPE *out) {\
    Cons *c = to_cons(*next);\
    if (c) {\
      *out = CONVERTER(c->head);\
      if (*out) {\
        *next = c->tail;\
        return 1;\
      }\
    }\
    return 0;\
  }

#define DEF_ACCEPT_ELEM2(NAME, RETURN_TYPE, TYPE_TEST, PROPERTY) \
  int accept_elem_ ## NAME (NseVal *next, RETURN_TYPE *out) {\
    Cons *c = to_cons(*next);\
    if (c) {\
      NseVal stripped = strip_syntax(c->head);\
      if (TYPE_TEST(stripped)) {\
        *out = stripped.PROPERTY;\
        *next = c->tail;\
        return 1;\
      }\
    }\
    return 0;\
  }

int accept_elem_any(NseVal *next, NseVal *out) {
  Cons *c = to_cons(*next);
  if (c) {
    if (out) {
      *out = c->head;
    }
    *next = c->tail;
    return 1;
  }
  return 0;
}

DEF_ACCEPT_ELEM(cons, Cons *, to_cons)
DEF_ACCEPT_ELEM(symbol, Symbol *, to_symbol)
DEF_ACCEPT_ELEM2(type_quote, TypeQuote *, is_type_quote, quote)

#define DEF_EXPECT_ELEM(NAME, RETURN_TYPE, ERROR) \
  RETURN_TYPE expect_elem_ ## NAME (NseVal *next) {\
    RETURN_TYPE out;\
    if (accept_elem_ ## NAME (next, &out)) {\
      return out;\
    }\
    set_debug_form(*next);\
    raise_error(syntax_error, ERROR);\
    return NULL;\
  }

DEF_EXPECT_ELEM(cons, Cons *, "expected a list")
DEF_EXPECT_ELEM(symbol, Symbol *, "expected a symbol")
DEF_EXPECT_ELEM(type_quote, TypeQuote *, "expected a type")

int expect_nil(NseVal *next) {
  if (!is_nil(*next)) {
    set_debug_form(*next);
    raise_error(syntax_error, "expected end of list");
    return 0;
  }
  return 1;
}
