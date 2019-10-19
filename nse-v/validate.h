/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_VALIDATE_H
#define NSE_VALIDATE_H

typedef struct Value Value;
typedef struct Symbol Symbol;
typedef struct Quote Quote;

#define V_EXACT(SYMBOL) (Validator){ .type = VALIDATOR_EXACT, .exact = (SYMBOL) }
#define V_SYMBOL(OUT) (Validator){ .type = VALIDATOR_SYMBOL, .symbol = (OUT) }
#define V_TQUOTE(OUT) (Validator){ .type = VALIDATOR_TQUOTE, .tquote = (OUT) }
#define V_ANY(OUT) (Validator){ .type = VALIDATOR_ANY, .any = (OUT) }
#define V_VECTOR(...) (Validator){ .type = VALIDATOR_VECTOR, .vector = (Validator []){ __VA_ARGS__, V_END }}
#define V_REP(REP) (Validator){ .type = VALIDATOR_REP, .rep = &(REP) }
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
    Vector **tquote;
    Value *any;
    Validator *vector;
    Validator *rep;
    Validator *alt;
  };
};

int validate(const Value value, Validator validator);

#endif
