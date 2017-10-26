#ifndef TYPE_H
#define TYPE_H

typedef enum {
  BASE_TYPE_ANY,
  BASE_TYPE_NIL,
  BASE_TYPE_I8,
  BASE_TYPE_I16,
  BASE_TYPE_I32,
  BASE_TYPE_I64,
  BASE_TYPE_U8,
  BASE_TYPE_U16,
  BASE_TYPE_U32,
  BASE_TYPE_U64,
  BASE_TYPE_F32,
  BASE_TYPE_F64,
  BASE_TYPE_STRING,
  BASE_TYPE_ANY_SYMBOL,
  BASE_TYPE_TYPE,
  BASE_TYPE_SYMBOL,
  BASE_TYPE_TYPE_VAR,
  BASE_TYPE_QUOTE,
  BASE_TYPE_TYPE_QUOTE,
  BASE_TYPE_SYNTAX,
  BASE_TYPE_CONS,
  BASE_TYPE_FUNC,
  BASE_TYPE_UNION,
  BASE_TYPE_RECUR
} BaseType;

typedef struct type Type;

struct type {
  size_t refs;
  BaseType type;
  union {
    char *var_name;
    Type *param_a;
  };
  Type *param_b;
};

int is_subtype_of(Type *a, Type *b);

#endif
