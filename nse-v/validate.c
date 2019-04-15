/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "error.h"
#include "write.h"
#include "../src/util/stream.h"

#include "validate.h"

static int validate_vector(Value value, Validator validators[]) {
  Value *cells;
  size_t length;
  if (syntax_is(value, VALUE_VECTOR)) {
    Vector *v = TO_VECTOR(syntax_get(value));
    cells = v->cells;
    length = v->length;
  } else if (syntax_is(value, VALUE_VECTOR_SLICE)) {
    VectorSlice *v = TO_VECTOR_SLICE(syntax_get(value));
    cells = v->cells;
    length = v->length;
  } else {
    set_debug_form(value);
    raise_error(syntax_error, "expected a vector");
    return 0;
  }
  size_t i = 0;
  while (validators->type != VALIDATOR_END) {
    if (validators->type == VALIDATOR_REP) {
      for ( ; i < length; i++) {
        if (!validate(cells[i], *validators->rep)) {
          return 0;
        }
      }
    } else if (i >= length) {
      return 0;
    } else if (!validate(cells[i], *validators)) {
      return 0;
    }
    validators++;
  }
  if (i < length) {
    set_debug_form(cells[i]);
    raise_error(syntax_error, "trailing elements");
    return 0;
  }
  return 1;
}

static void validator_to_stream(Validator validator, Stream *stream) {
  switch (validator.type) {
    case VALIDATOR_EXACT: {
      char *sym = nse_write_to_string(SYMBOL(validator.exact), validator.exact->module);
      stream_printf(stream, sym);
      free(sym);
      break;
    }
    case VALIDATOR_SYMBOL:
      stream_printf(stream, "SYMBOL");
      break;
    case VALIDATOR_TQUOTE:
      stream_printf(stream, "^TYPE");
      break;
    case VALIDATOR_ANY:
      stream_printf(stream, "EXPR");
      break;
    case VALIDATOR_VECTOR: {
      stream_printf(stream, "(");
      Validator *validators = validator.vector;
      if (validators->type != VALIDATOR_END) {
        validator_to_stream(*validators, stream);
        validators++;
      }
      while (validators->type != VALIDATOR_END) {
        stream_printf(stream, " ");
        validator_to_stream(*validators, stream);
        validators++;
      }
      stream_printf(stream, ")");
      break;
    }
    case VALIDATOR_REP:
      stream_printf(stream, "{");
      validator_to_stream(*validator.rep, stream);
      stream_printf(stream, "}");
      break;
    case VALIDATOR_ALT: {
      Validator *validators = validator.alt;
      if (validators->type != VALIDATOR_END) {
        validator_to_stream(*validators, stream);
        validators++;
      }
      while (validators->type != VALIDATOR_END) {
        stream_printf(stream, "|");
        validator_to_stream(*validators, stream);
        validators++;
      }
      break;
    }
    default:
      break;
  }
}

static char *validator_to_string(Validator validator) {
  size_t size = 32;
  char *buffer = (char *)malloc(size);
  Stream *stream = stream_buffer(buffer, size, 0);
  validator_to_stream(validator, stream);
  buffer = stream_get_content(stream);
  stream_close(stream);
  return buffer;
}

int validate(const Value value, Validator validator) {
  switch (validator.type) {
    case VALIDATOR_EXACT:
      if (syntax_is(value, VALUE_SYMBOL) && TO_SYMBOL(syntax_get(value)) == validator.exact) {
        return 1;
      }
      raise_error(syntax_error, "expected \"%s\"", TO_C_STRING(validator.exact->name));
      break;
    case VALIDATOR_SYMBOL: 
      if (syntax_is(value, VALUE_SYMBOL)) {
        *validator.symbol = TO_SYMBOL(syntax_get(value));
        return 1;
      }
      raise_error(syntax_error, "expected a symbol");
      break;
    case VALIDATOR_TQUOTE: {
      if (syntax_is(value, VALUE_TYPE_QUOTE)) {
        *validator.tquote = TO_QUOTE(syntax_get(value));
        return 1;
      }
      raise_error(syntax_error, "expected a type");
      break;
    }
    case VALIDATOR_ANY:
      *validator.any = syntax_get(value);
      return 1;
    case VALIDATOR_VECTOR: {
      if (validate_vector(value, validator.vector)) {
        return 1;
      }
      char *form = validator_to_string(validator);
      raise_error(syntax_error, "expected %s", form);
      free(form);
      return 0;
    }
    case VALIDATOR_ALT: {
      for (Validator *alt = validator.alt; alt->type != VALIDATOR_END; alt++) {
        if (validate(value, *alt)) {
          return 1;
        }
      }
      char *form = validator_to_string(validator);
      raise_error(syntax_error, "expected %s", form);
      free(form);
      break;
    }
    default:
      return 1;
  }
  set_debug_form(value);
  return 0;
}

