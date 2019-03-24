#ifndef NSE_VALIDATE_H
#define NSE_VALIDATE_H

#include "value.h"

#define V_EXACT(symbol) (Validator){ .type = VALIDATOR_EXACT, .exact = symbol }
#define V_SYMBOL(out) (Validator){ .type = VALIDATOR_SYMBOL, .symbol = out }
#define V_ANY(out) (Validator){ .type = VALIDATOR_ANY, .any = out }
#define V_END (Validator){ .type = VALIDATOR_END }
#define VALIDATE(list, ...) validate_list(list, (Validator []){ __VA_ARGS__, V_END })


typedef struct {
  enum {
    VALIDATOR_EXACT,
    VALIDATOR_SYMBOL,
    VALIDATOR_ANY,
    VALIDATOR_END
  } type;
  union {
    Symbol *exact;
    Symbol **symbol;
    NseVal *any;
  };
} Validator;

int validate_list(NseVal value, Validator validators[]);

#endif
