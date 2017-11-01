#include <stdlib.h>
#include <string.h>

#include "error.h"

#include "type.h"

typedef struct subst Subst;

struct subst {
  const char *name;
  const Type *type;
  Subst *next;
};

Type *nothing_type = &(Type){ .refs = 1, .type = BASE_TYPE_NOTHING };
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
Type *keyword_type = &(Type){ .refs = 1, .type = BASE_TYPE_KEYWORD };
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

int type_is_product(const Type *t) {
  if (t->type == BASE_TYPE_PRODUCT || t->type == BASE_TYPE_NIL) {
    return 1;
  } else if (t->type == BASE_TYPE_CONS) {
    return type_is_product(t->param_b);
  }
  return 0;
}

int type_equals(const Type *a, const Type *b) {
  if (a->type == BASE_TYPE_ALIAS) {
    return type_equals(a->param_b, b);
  }
  if (b->type == BASE_TYPE_ALIAS) {
    return type_equals(a, b->param_b);
  }
  if (a->type != b->type) {
    return 0;
  }
  switch (a->type) {
    case BASE_TYPE_PRODUCT:
      if (a->num_operands != b->num_operands) {
        return 0;
      }
      for (size_t i = 0; i < a->num_operands; i++) {
        if (!type_equals(a->operands[i], b->operands[i])) {
          return 0;
        }
      }
      return 1;
    case BASE_TYPE_CONS:
    case BASE_TYPE_FUNC:
    case BASE_TYPE_UNION:
      if (!type_equals(a->param_b, b->param_b)) {
        return 0;
      }
    case BASE_TYPE_QUOTE:
    case BASE_TYPE_TYPE_QUOTE:
    case BASE_TYPE_SYNTAX:
      return type_equals(a->param_a, b->param_a);
    case BASE_TYPE_RECUR:
    case BASE_TYPE_FORALL:
      if (!type_equals(a->param_b, b->param_b)) {
        return 0;
      }
    case BASE_TYPE_SYMBOL:
    case BASE_TYPE_TYPE_VAR:
      return strcmp(a->var_name, b->var_name) == 0;
    default:
      return 1;
  }

}


static int is_subtype_of_s(const Type *a, const Type *b, Subst *s);
static int is_subtype_of_s(const Type *a, const Type *b, Subst *s) {
  if (a->type == BASE_TYPE_TYPE_VAR) {
    const Type *replacement = apply_substitution(a->var_name, s);
    if (replacement) {
      return is_subtype_of_s(replacement, b, s);
    }
    return 0;
  } else if (a->type == BASE_TYPE_ALIAS) {
    return is_subtype_of_s(a->param_b, b, s);
  } else if (a->type == BASE_TYPE_UNION) {
    return is_subtype_of_s(a->param_a, b, s) && is_subtype_of_s(a->param_b, b, s);
  } else if (a->type == BASE_TYPE_NOTHING) {
    return 1;
  }
  switch (b->type) {
    case BASE_TYPE_NOTHING:
      return 0;
    case BASE_TYPE_ANY:
      return 1;
    case BASE_TYPE_NIL:
      return a->type == BASE_TYPE_NIL || (a->type == BASE_TYPE_PRODUCT && a->num_operands == 0);
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
    case BASE_TYPE_KEYWORD:
      return a->type == BASE_TYPE_KEYWORD;
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
      if (a->type == BASE_TYPE_PRODUCT) {
        for (size_t i = 0; i < a->num_operands; i++) {
          if (b->type != BASE_TYPE_CONS) {
            return 0;
          } else if (!is_subtype_of_s(a->operands[i], b->param_a, s)) {
            return 0;
          }
          b = b->param_b;
        }
        if (b->type != BASE_TYPE_NIL) {
          return 0;
        }
        return 1;
      } else if (a->type == BASE_TYPE_CONS) {
        return is_subtype_of_s(a->param_a, b->param_a, s) && is_subtype_of_s(a->param_b, b->param_b, s);
      } else {
        return 0;
      }
    case BASE_TYPE_FUNC:
      return a->type == BASE_TYPE_FUNC && is_subtype_of_s(b->param_a, a->param_a, s) && is_subtype_of_s(a->param_b, b->param_b, s);
    case BASE_TYPE_UNION:
      return is_subtype_of_s(a, b->param_a, s) || is_subtype_of_s(a, b->param_b, s);
    case BASE_TYPE_PRODUCT:
      if (a->type == BASE_TYPE_PRODUCT) {
        if (a->num_operands != b->num_operands) {
          return 0;
        }
        for (size_t i = 0; i < a->num_operands; i++) {
          if (!is_subtype_of_s(a->operands[i], b->operands[i], s)) {
            return 0;
          }
        }
        return 1;
      } else if (a->type == BASE_TYPE_CONS) {
        for (size_t i = 0; i < b->num_operands; i++) {
          if (a->type != BASE_TYPE_CONS) {
            return 0;
          } else if (!is_subtype_of_s(a->param_a, b->operands[i], s)) {
            return 0;
          }
          a = a->param_b;
        }
        if (a->type != BASE_TYPE_NIL) {
          return 0;
        }
        return 1;
      } else {
        return 0;
      }
    case BASE_TYPE_RECUR: {
      Subst *new_s = add_substitution(b->var_name, b, s);
      int result = is_subtype_of_s(a, b->param_b, new_s);
      delete_substitution(new_s);
      return result;
    }
    case BASE_TYPE_ALIAS:
      return is_subtype_of_s(a, b->param_b, s);
    case BASE_TYPE_FORALL: {
      Subst *new_s = add_substitution(b->var_name, any_type, s);
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
  if (param_a) {
    Type *t = allocate(sizeof(Type));
    if (t) {
      t->refs = 1;
      t->type = type;
      t->param_a = param_a;
      t->param_b = NULL;
      return t;
    }
  }
  delete_type(param_a);
  return NULL;
}

static Type *create_unary_type_name(BaseType type, const char *name) {
  if (name) {
    Type *t = allocate(sizeof(Type));
    if (t) {
      t->refs = 1;
      t->type = type;
      size_t len = strlen(name);
      char *copy = allocate(len + 1);
      if (copy) {
        memcpy(copy, name, len);
        copy[len] = '\0';
        t->var_name = copy;
        t->param_b = NULL;
        return t;
      }
      free(t);
    }
  }
  return NULL;
}

static Type *create_binary_type(BaseType type, Type *param_a, Type *param_b) {
  if (param_a && param_b) {
    Type *t = allocate(sizeof(Type));
    if (t) {
      t->refs = 1;
      t->type = type;
      t->param_a = param_a;
      t->param_b = param_b;
      return t;
    }
  }
  delete_type(param_a);
  delete_type(param_b);
  return NULL;
}

static Type *create_binary_type_name(BaseType type, const char *name, Type *body) {
  if (name && body) {
    Type *t = allocate(sizeof(Type));
    if (t) {
      t->refs = 1;
      t->type = type;
      size_t len = strlen(name);
      t->var_name = allocate(len + 1);
      if (t->var_name) {
        memcpy(t->var_name, name, len);
        t->var_name[len] = '\0';
        t->param_b = body;
        return t;
      }
      free(t);
    }
  }
  delete_type(body);
  return NULL;
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

Type *create_product_type(Type *operands[], size_t num_operands) {
  if (operands) {
    Type *t = allocate(sizeof(Type));
    if (t) {
      t->refs = 1;
      t->type = BASE_TYPE_PRODUCT;
      t->num_operands = num_operands;
      if (num_operands > 0) {
        t->operands = allocate(sizeof(Type *) * num_operands);
        if (t->operands) {
          memcpy(t->operands, operands, sizeof(Type *) * num_operands);
          return t;
        }
      } else {
        t->operands = NULL;
        return t;
      }
      free(t);
    }
    for (size_t i = 0; i < num_operands; i++) {
      delete_type(operands[i]);
    }
  }
  return NULL;
}

Type *create_recur_type(const char *name, Type *t) {
  return create_binary_type_name(BASE_TYPE_RECUR, name, t);
}

Type *create_alias_type(const char *name, Type *t) {
  return create_binary_type_name(BASE_TYPE_ALIAS, name, t);
}

Type *create_forall_type(const char *name, Type *t) {
  return create_binary_type_name(BASE_TYPE_FORALL, name, t);
}

Type *copy_type(Type *t) {
  t->refs++;
  return t;
}

void delete_type(Type *t) {
  if (!t) {
    return;
  }
  if (t->refs > 0) {
    t->refs--;
  }
  if (t->refs == 0) {
    switch (t->type) {
      case BASE_TYPE_PRODUCT:
        for (size_t i = 0; i < t->num_operands; i++) {
          delete_type(t->operands[i]);
        }
        free(t->operands);
        break;
      case BASE_TYPE_CONS:
      case BASE_TYPE_FUNC:
      case BASE_TYPE_UNION:
        delete_type(t->param_b);
      case BASE_TYPE_QUOTE:
      case BASE_TYPE_TYPE_QUOTE:
      case BASE_TYPE_SYNTAX:
        delete_type(t->param_a);
        break;
      case BASE_TYPE_FORALL:
      case BASE_TYPE_RECUR:
      case BASE_TYPE_ALIAS:
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
    case BASE_TYPE_NOTHING:
      return "nothing";
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
    case BASE_TYPE_KEYWORD:
      return "keyword";
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
      return "->";
    case BASE_TYPE_UNION:
      return "+";
    case BASE_TYPE_PRODUCT:
      return "*";
    case BASE_TYPE_RECUR:
      return "µ";
    case BASE_TYPE_ALIAS:
      return "alias";
    case BASE_TYPE_FORALL:
      return "forall";
  }
}

Type *simplify_type(Type *t) {
  switch (t->type) {
    case BASE_TYPE_PRODUCT: {
      Type *copy = create_product_type(NULL, 0);
      if (copy) {
        copy->num_operands = t->num_operands;
        copy->operands = allocate(sizeof(Type *) * t->num_operands);
        if (copy->operands) {
          size_t i = 0;
          for (size_t i = 0; i < t->num_operands; i++) {
            copy->operands[i] = simplify_type(t->operands[i]);
            if (!copy->operands[i]) {
              break;
            }
          }
          if (i == t->num_operands) {
            return copy;
          }
          for (size_t j = 0; j < i; j++) {
            delete_type(copy->operands[i]);
          }
        }
        free(copy);
      }
      return NULL;
    }
    case BASE_TYPE_CONS:
    case BASE_TYPE_FUNC:
      return create_binary_type(t->type, simplify_type(t->param_a), simplify_type(t->param_b));
    case BASE_TYPE_UNION: {
      Type *a = simplify_type(t->param_a);
      Type *b = simplify_type(t->param_b);
      if (a && b) {
        if (is_subtype_of(a, b)) {
          return b;
        } else if (is_subtype_of(b, a)) {
          return a;
        } else {
          return create_union_type(a, b);
        }
      }
      delete_type(a);
      delete_type(b);
      return NULL;
    }
    case BASE_TYPE_QUOTE:
    case BASE_TYPE_TYPE_QUOTE:
    case BASE_TYPE_SYNTAX:
      return create_unary_type(t->type, simplify_type(t->param_a));
    case BASE_TYPE_RECUR:
    case BASE_TYPE_ALIAS:
    case BASE_TYPE_FORALL:
      return create_binary_type_name(t->type, t->var_name, t->param_b);
    default:
      return copy_type(t);
  }
}

