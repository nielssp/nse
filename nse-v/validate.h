/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_VALIDATE_H
#define NSE_VALIDATE_H

typedef struct Value Value;
typedef struct Symbol Symbol;
typedef struct Quote Quote;

#define V_EXACT(symbol) (Validator){ .type = VALIDATOR_EXACT, .exact = symbol }
#define V_SYMBOL(out) (Validator){ .type = VALIDATOR_SYMBOL, .symbol = out }
#define V_TQUOTE(out) (Validator){ .type = VALIDATOR_TQUOTE, .tquote = out }
#define V_ANY(out) (Validator){ .type = VALIDATOR_ANY, .any = out }
#define V_VECTOR(...) (Validator){ .type = VALIDATOR_VECTOR, .alt = (Validator []){ __VA_ARGS__, V_END }}
#define V_REP(rep) (Validator){ .type = VALIDATOR_REP, .rep = &rep }
#define V_ALT(...) (Validator){ .type = VALIDATOR_ALT, .alt = (Validator []){ __VA_ARGS__, V_END }}
#define V_END (Validator){ .type = VALIDATOR_END }

typedef struct Validator Validator;

struct Validator {
  enum {
    VALIDATOR_EXACT,
    VALIDATOR_SYMBOL,
    VALIDATOR_TQUOTE,
    VALIDATOR_ANY,
    VALIDATOR_VECTOR,
    VALIDATOR_REP,
    VALIDATOR_ALT,
    VALIDATOR_END
  } type;
  union {
    Symbol *exact;
    Symbol **symbol;
    Quote **tquote;
    Value *any;
    Validator *vector;
    Validator *rep;
    Validator *alt;
  };
};

int validate(const Value value, Validator validator);

#endif
