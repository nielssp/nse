#include "validate.h"

static int validate(NseVal value, Validator validator) {
  if (value.type == syntax_type) {
    value = syntax_to_datum(value);
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
