#ifndef NSE_TYPE_H
#define NSE_TYPE_H

typedef struct CType CType;
typedef struct GType GType;

typedef enum {
  INTERNAL_NOTHING,
  INTERNAL_NIL,
  INTERNAL_CONS,
  INTERNAL_I64,
  INTERNAL_F64,
  INTERNAL_FUNC,
  INTERNAL_CLOSURE,
  INTERNAL_STRING,
  INTERNAL_SYNTAX,
  INTERNAL_SYMBOL,
  INTERNAL_REFERENCE,
  INTERNAL_TYPE,
  INTERNAL_QUOTE,
} InternalType;

typedef enum {
  C_TYPE_SIMPLE,
  C_TYPE_FUNC,
  C_TYPE_CLOSURE,
  C_TYPE_INSTANCE,
} CTypeType;

struct CType {
  size_t refs;
  CTypeType type;
  InternalType internal;
  CType *super;
  union {
    struct {
      GType *type;
      CType **parameters;
    } instance;
    struct {
      int min_arity;
      int variadic;
    } func;
  };
};

typedef struct GType Gtype;

extern CType *any_type;
extern CType *improper_list_type;
extern CType *any_list_type;
extern CType *nil_type;
extern CType *cons_type;
extern CType *num_type;
extern CType *i64_type;
extern CType *f64_type;
extern CType *string_type;
extern CType *symbol_type;
extern CType *keyword_type;
extern CType *quote_type;
extern CType *continue_type;
extern CType *type_quote_type;
extern CType *type_type;
extern CType *syntax_type;
extern CType *func_type;
extern CType *scope_type;

extern GType *list_type;

void init_types();

CType *create_simple_type(InternalType internal, CType *super);
GType *create_generic(int arity, InternalType internal, CType *super);

CType *copy_type(CType *t);
void delete_type(CType *t);

CType *get_instance(GType *g, CType **parameters);
CType *get_unary_instance(GType *g, CType *parameter);
CType *get_func_type(int min_arity, int variadic);
CType *get_closure_type(int min_arity, int variadic);

CType *get_super_type(CType *t);

#endif


