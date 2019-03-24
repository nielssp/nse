#include "validate.h"

int validate_list(NseVal value, Validator validators[]) {
  while (validators->type != VALIDATOR_END) {
    if (!is_cons(value)) {
      return 0;
    }
    NseVal h = head(value);
    value = tail(value);
    switch (validators->type) {
      case VALIDATOR_EXACT: {
        Symbol *symbol = to_symbol(h);
        if (!symbol || symbol != validators->exact) {
          return 0;
        }
        break;
      }
      case VALIDATOR_SYMBOL: {
        Symbol *symbol = to_symbol(h);
        if (!symbol) {
          return 0;
        }
        *validators->symbol = symbol;
        break;
      }
      case VALIDATOR_ANY:
        *validators->any = h;
        break;
      case VALIDATOR_END:
        break;
    }
    validators++;
  }
  return is_nil(value);
}
