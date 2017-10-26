#include <stdlib.h>
#include <string.h>

#include "type.h"

typedef struct subst Subst;

struct subst {
  const char *name;
  const Type *type;
  Subst *next;
};

Type *any_type = &(Type){ .refs = 1, .type = BASE_TYPE_ANY };
Type *nil_type = &(Type){ .refs = 1, .type = BASE_TYPE_NIL };
Type *ref_type = &(Type){ .refs = 1, .type = BASE_TYPE_REF };
Type *i8_type = &(Type){ .refs = 1, .type = BASE_TYPE_I8 };
Type *i16_type = &(Type){ .refs = 1, .type = BASE_TYPE_I16 };
Type *i32_type = &(Type){ .refs = 1, .type = BASE_TYPE_I32 };
Type *i64_type = &(Type){ .refs = 1, .type = BASE_TYPE_I64 };
Type *u8_type = &(Type){ .refs = 1, .type = BASE_TYPE_U8 };
Type *u16_type = &(Type){ .refs = 1, .type = BASE_TYPE_U16 };
Type *u32_type = &(Type){ .refs = 1, .type = BASE_TYPE_U32 };
Type *u64_type = &(Type){ .refs = 1, .type = BASE_TYPE_U64 };
Type *f32_type = &(Type){ .refs = 1, .type = BASE_TYPE_F32 };
Type *f64_type = &(Type){ .refs = 1, .type = BASE_TYPE_F64 };
Type *string_type = &(Type){ .refs = 1, .type = BASE_TYPE_STRING };
Type *any_symbol_type = &(Type){ .refs = 1, .type = BASE_TYPE_ANY_SYMBOL };
Type *type_type = &(Type){ .refs = 1, .type = BASE_TYPE_TYPE };

static Subst *add_substitution(const char *name, const Type *type, Subst *next) {
  Subst *s = malloc(sizeof(Subst));
  s->name = name;
  s->type = type;
  s->next = next;
  return s;
}

static Subst *delete_substitution(Subst *s) {
  if (s) {
    Subst *next = s->next;
    free(s);
    return next;
  }
  return NULL;
}

static const Type *apply_substitution(const char *name, Subst *s) {
  if (s == NULL) {
    return NULL;
  } else if (strcmp(name, s->name) == 0) {
    return s->type;
  }
  return NULL;
}

static int is_subtype_of_s(const Type *a, const Type *b, Subst *s);
static int is_subtype_of_s(const Type *a, const Type *b, Subst *s) {
  switch (b->type) {
    case BASE_TYPE_ANY:
      return 1;
    case BASE_TYPE_NIL:
      return a->type == BASE_TYPE_NIL;
    case BASE_TYPE_REF:
      return a->type == BASE_TYPE_REF;
    case BASE_TYPE_I64:
      if (a->type == BASE_TYPE_I64 || a->type == BASE_TYPE_U32) {
        return 1;
      }
    case BASE_TYPE_I32:
      if (a->type == BASE_TYPE_I32 || a->type == BASE_TYPE_U16) {
        return 1;
      }
    case BASE_TYPE_I16:
      if (a->type == BASE_TYPE_I16 || a->type == BASE_TYPE_U8) {
        return 1;
      }
    case BASE_TYPE_I8:
      return a->type == BASE_TYPE_I8;
    case BASE_TYPE_U64:
      if (a->type == BASE_TYPE_U64) {
        return 1;
      }
    case BASE_TYPE_U32:
      if (a->type == BASE_TYPE_U32) {
        return 1;
      }
    case BASE_TYPE_U16:
      if (a->type == BASE_TYPE_U16) {
        return 1;
      }
    case BASE_TYPE_U8:
      return a->type == BASE_TYPE_U8;
    case BASE_TYPE_F64:
      if (a->type == BASE_TYPE_F64) {
        return 1;
      }
    case BASE_TYPE_F32:
      return a->type == BASE_TYPE_F32;
    case BASE_TYPE_STRING:
      return a->type == BASE_TYPE_STRING;
    case BASE_TYPE_ANY_SYMBOL:
      return a->type == BASE_TYPE_SYMBOL || a->type == BASE_TYPE_ANY_SYMBOL;
    case BASE_TYPE_TYPE:
      return a->type == BASE_TYPE_TYPE;
    case BASE_TYPE_SYMBOL:
      return a->type == BASE_TYPE_SYMBOL && strcmp(a->var_name, b->var_name) == 0;
    case BASE_TYPE_TYPE_VAR: {
      const Type *replacement = apply_substitution(b->var_name, s);
      if (replacement) {
        return is_subtype_of_s(a, replacement, s);
      }
      return 0;
    }
    case BASE_TYPE_QUOTE:
      return a->type == BASE_TYPE_QUOTE && is_subtype_of_s(a->param_a, b->param_a, s);
    case BASE_TYPE_TYPE_QUOTE:
      return a->type == BASE_TYPE_TYPE_QUOTE && is_subtype_of_s(a->param_a, b->param_a, s);
    case BASE_TYPE_SYNTAX:
      return a->type == BASE_TYPE_SYNTAX && is_subtype_of_s(a->param_a, b->param_a, s);
    case BASE_TYPE_CONS:
      return a->type == BASE_TYPE_CONS && is_subtype_of_s(a->param_a, b->param_a, s) && is_subtype_of_s(a->param_b, b->param_b, s);
    case BASE_TYPE_FUNC:
      return a->type == BASE_TYPE_FUNC && is_subtype_of_s(a->param_a, b->param_a, s) && is_subtype_of_s(a->param_b, b->param_b, s);
    case BASE_TYPE_UNION:
      return is_subtype_of_s(a, b->param_a, s) || is_subtype_of_s(a, b->param_b, s);
    case BASE_TYPE_RECUR: {
      Subst *new_s = add_substitution(b->var_name, b, s);
      int result = is_subtype_of_s(a, b->param_b, new_s);
      delete_substitution(new_s);
      return result;
    }
  }
}

int is_subtype_of(const Type *a, const Type *b) {
  return is_subtype_of_s(a, b, NULL);
}

static Type *create_unary_type(BaseType type, Type *param_a) {
  Type *t = malloc(sizeof(Type));
  t->refs = 1;
  t->type = type;
  t->param_a = param_a;
  t->param_b = NULL;
  return t;
}

static Type *create_unary_type_name(BaseType type, const char *name) {
  Type *t = malloc(sizeof(Type));
  t->refs = 1;
  t->type = type;
  size_t len = strlen(name);
  char *copy = malloc(len + 1);
  if (!copy) {
    return NULL;
  }
  memcpy(copy, name, len);
  copy[len] = '\0';
  t->var_name = copy;
  t->param_b = NULL;
  return t;
}

static Type *create_binary_type(BaseType type, Type *param_a, Type *param_b) {
  Type *t = malloc(sizeof(Type));
  t->refs = 1;
  t->type = type;
  t->param_a = param_a;
  t->param_b = param_b;
  return t;
}

Type *create_symbol_type(const char *symbol) {
  return create_unary_type_name(BASE_TYPE_SYMBOL, symbol);
}

Type *create_type_var(const char *var_name) {
  return create_unary_type_name(BASE_TYPE_TYPE_VAR, var_name);
}

Type *create_quote_type(Type *quoted_type) {
  return create_unary_type(BASE_TYPE_QUOTE, quoted_type);
}

Type *create_type_quote_type(Type *quoted_type) {
  return create_unary_type(BASE_TYPE_TYPE_QUOTE, quoted_type);
}

Type *create_syntax_type(Type *quoted_type) {
  return create_unary_type(BASE_TYPE_SYNTAX, quoted_type);
}

Type *create_cons_type(Type *head_type, Type *tail_type) {
  return create_binary_type(BASE_TYPE_CONS, head_type, tail_type);
}

Type *create_func_type(Type *arg_type, Type *return_type) {
  return create_binary_type(BASE_TYPE_FUNC, arg_type, return_type);
}

Type *create_union_type(Type *type_a, Type *type_b) {
  return create_binary_type(BASE_TYPE_UNION, type_a, type_b);
}

Type *create_recur_type(const char *name, Type *body) {
  Type *t = malloc(sizeof(Type));
  t->refs = 1;
  t->type = BASE_TYPE_RECUR;
  size_t len = strlen(name);
  char *copy = malloc(len + 1);
  if (!copy) {
    return NULL;
  }
  memcpy(copy, name, len);
  copy[len] = '\0';
  t->var_name = copy;
  t->param_b = body;
  return t;
}

Type *copy_type(Type *t) {
  t->refs++;
  return t;
}

void delete_type(Type *t) {
  if (t->refs > 0) {
    t->refs--;
  }
  if (t->refs == 0) {
    switch (t->type) {
      case BASE_TYPE_CONS:
      case BASE_TYPE_FUNC:
      case BASE_TYPE_UNION:
        delete_type(t->param_b);
      case BASE_TYPE_QUOTE:
      case BASE_TYPE_TYPE_QUOTE:
      case BASE_TYPE_SYNTAX:
        delete_type(t->param_a);
        break;
      case BASE_TYPE_RECUR:
        delete_type(t->param_b);
      case BASE_TYPE_SYMBOL:
      case BASE_TYPE_TYPE_VAR:
        free(t->var_name);
        break;
      default:
        break;
    }
    free(t);
  }
}

const char *base_type_to_string(BaseType t) {
  switch (t) {
    case BASE_TYPE_ANY:
      return "any";
    case BASE_TYPE_NIL:
      return "nil";
    case BASE_TYPE_REF:
      return "ref";
    case BASE_TYPE_I8:
      return "i8";
    case BASE_TYPE_I16:
      return "i16";
    case BASE_TYPE_I32:
      return "i32";
    case BASE_TYPE_I64:
      return "i64";
    case BASE_TYPE_U8:
      return "u8";
    case BASE_TYPE_U16:
      return "u16";
    case BASE_TYPE_U32:
      return "u32";
    case BASE_TYPE_U64:
      return "u64";
    case BASE_TYPE_F32:
      return "f32";
    case BASE_TYPE_F64:
      return "f64";
    case BASE_TYPE_STRING:
      return "string";
    case BASE_TYPE_ANY_SYMBOL:
      return "any-symbol";
    case BASE_TYPE_TYPE:
      return "type";
    case BASE_TYPE_SYMBOL:
      return "symbol";
    case BASE_TYPE_TYPE_VAR:
      return "type-var";
    case BASE_TYPE_QUOTE:
      return "quote";
    case BASE_TYPE_TYPE_QUOTE:
      return "type-quote";
    case BASE_TYPE_SYNTAX:
      return "syntax";
    case BASE_TYPE_CONS:
      return "cons";
    case BASE_TYPE_FUNC:
      return "→";
    case BASE_TYPE_UNION:
      return "∪";
    case BASE_TYPE_RECUR:
      return "µ";
  }
}