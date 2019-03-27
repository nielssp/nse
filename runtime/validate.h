#ifndef NSE_VALIDATE_H
#define NSE_VALIDATE_H

#include "value.h"

#define V_EXACT(symbol) (Validator){ .type = VALIDATOR_EXACT, .exact = symbol }
#define V_SYMBOL(out) (Validator){ .type = VALIDATOR_SYMBOL, .symbol = out }
#define V_TQUOTE(out) (Validator){ .type = VALIDATOR_TQUOTE, .tquote = out }
#define V_ANY(out) (Validator){ .type = VALIDATOR_ANY, .any = out }
#define V_LIST(...) (Validator){ .type = VALIDATOR_LIST, .alt = (Validator []){ __VA_ARGS__, V_END }}
#define V_REP(rep) (Validator){ .type = VALIDATOR_REP, .rep = &rep }
#define V_ALT(...) (Validator){ .type = VALIDATOR_ALT, .alt = (Validator []){ __VA_ARGS__, V_END }}
#define V_END (Validator){ .type = VALIDATOR_END }
#define VALIDATE(list, ...) validate_list(list, (Validator []){ __VA_ARGS__, V_END })

typedef struct Validator Validator;

struct Validator {
  enum {
    VALIDATOR_EXACT,
    VALIDATOR_SYMBOL,
    VALIDATOR_TQUOTE,
    VALIDATOR_ANY,
    VALIDATOR_LIST,
    VALIDATOR_REP,
    VALIDATOR_ALT,
    VALIDATOR_END
  } type;
  union {
    Symbol *exact;
    Symbol **symbol;
    TypeQuote **tquote;
    NseVal *any;
    Validator *list;
    Validator *rep;
    Validator *alt;
  };
};

int validate_list(NseVal value, Validator validators[]);

#endif
