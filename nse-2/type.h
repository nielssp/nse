#ifndef TYPE_H
#define TYPE_H

typedef enum {
  BASE_TYPE_ANY,
  BASE_TYPE_NIL,
  BASE_TYPE_REF,
  BASE_TYPE_I8,
  BASE_TYPE_I16,
  BASE_TYPE_I32,
  BASE_TYPE_I64,
  BASE_TYPE_U8,
  BASE_TYPE_U16,
  BASE_TYPE_U32,
  BASE_TYPE_U64,
  BASE_TYPE_F32, BASE_TYPE_F64,
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

extern Type *any_type;
extern Type *nil_type;
extern Type *ref_type;
extern Type *i8_type;
extern Type *i16_type;
extern Type *i32_type;
extern Type *i64_type;
extern Type *u8_type;
extern Type *u16_type;
extern Type *u32_type;
extern Type *u64_type;
extern Type *f32_type;
extern Type *f64_type;
extern Type *string_type;
extern Type *any_symbol_type;
extern Type *type_type;

Type *create_symbol_type(const char *symbol);
Type *create_type_var(const char *var_name);
Type *create_quote_type(Type *quoted_type);
Type *create_type_quote_type(Type *quoted_type);
Type *create_syntax_type(Type *quoted_type);
Type *create_cons_type(Type *head_type, Type *tail_type);
Type *create_func_type(Type *arg_type, Type *return_type);
Type *create_union_type(Type *type_a, Type *type_b);
Type *create_recur_type(const char *name, Type *t);

Type *copy_type(Type *t);
void delete_type(Type *t);

int is_subtype_of(const Type *a, const Type *b);

const char *base_type_to_string(BaseType t);

#endif
