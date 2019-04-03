/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "hashmap.h"
#include "value.h"

#include "type.h"

typedef struct FuncType FuncType;

DECLARE_HASH_MAP(instance_map, InstanceMap, CType **, CType *)
DECLARE_HASH_MAP(func_type_map, FuncTypeMap, FuncType *, CType *)

/* A generic type. */
struct GType {
  size_t refs;
  int arity;
  /* Optional name. */
  Symbol *name;
  /* Internal type. */
  InternalType internal;
  /* Optional super type. */
  CType *super;
  /* Hash map of instances. Instances are weakly referenced. When an instance is
   * deleted (refs=0) it is automatically removed from the instance map of the
   * corresponding generic type by `delete_type()`. */
  InstanceMap instances;
  /* Optional weak reference to polymorphic instance. When the instance is
   * deleted (i.e. `poly->refs == 0`), the field is automatically set to NULL by
   * `delete_type()`. */
  CType *poly;
};

/* Structure used as a key in function type hash maps. */
struct FuncType {
  int min_arity;
  int variadic;
};

/* Map of function type instances. */
FuncTypeMap func_types;
/* Map of closure type instances. */
FuncTypeMap closure_types;

CType *any_type;
CType *bool_type;
CType *proper_list_type;
CType *improper_list_type;
CType *nil_type;
CType *num_type;
CType *int_type;
CType *float_type;
CType *i64_type;
CType *f64_type;
CType *string_type;
CType *symbol_type;
CType *keyword_type;
CType *quote_type;
CType *continue_type;
CType *type_quote_type;
CType *syntax_type;
CType *type_type;
CType *func_type;
CType *scope_type;
CType *stream_type;
CType *generic_type_type;

GType *list_type;

void init_types() {
  func_types = create_func_type_map();
  closure_types = create_func_type_map();
  any_type = create_simple_type(INTERNAL_NOTHING, NULL);
  bool_type = create_simple_type(INTERNAL_DATA, any_type);
  improper_list_type = create_simple_type(INTERNAL_CONS, any_type);
  proper_list_type = create_simple_type(INTERNAL_NOTHING, improper_list_type);
  list_type = create_generic(1, INTERNAL_CONS, proper_list_type);
  nil_type = create_simple_type(INTERNAL_NIL, get_poly_instance(copy_generic(list_type)));
  num_type = create_simple_type(INTERNAL_NOTHING, any_type);
  int_type = create_simple_type(INTERNAL_I64, num_type);
  float_type = create_simple_type(INTERNAL_F64, num_type);
  i64_type = create_simple_type(INTERNAL_I64, int_type);
  f64_type = create_simple_type(INTERNAL_F64, float_type);
  string_type = create_simple_type(INTERNAL_STRING, any_type);
  symbol_type = create_simple_type(INTERNAL_SYMBOL, any_type);
  keyword_type = create_simple_type(INTERNAL_SYMBOL, any_type);
  quote_type = create_simple_type(INTERNAL_QUOTE, any_type);
  continue_type = create_simple_type(INTERNAL_QUOTE, any_type);
  type_quote_type = create_simple_type(INTERNAL_QUOTE, any_type);
  syntax_type = create_simple_type(INTERNAL_SYNTAX, any_type);
  type_type = create_simple_type(INTERNAL_TYPE, any_type);
  func_type = create_simple_type(INTERNAL_NOTHING, any_type);
  scope_type = create_simple_type(INTERNAL_REFERENCE, any_type);
  stream_type = create_simple_type(INTERNAL_REFERENCE, any_type);
  generic_type_type = create_simple_type(INTERNAL_REFERENCE, any_type);
}

CType *create_simple_type(InternalType internal, CType *super) {
  CType *t = allocate(sizeof(CType));
  if (!t) {
    delete_type(super);
    return NULL;
  }
  t->refs = 1;
  t->type = C_TYPE_SIMPLE;
  t->internal = internal;
  t->super = super;
  t->name = NULL;
  return t;
}

GType *create_generic(int arity, InternalType internal, CType *super) {
  GType *t = allocate(sizeof(GType));
  if (!t) {
    delete_type(super);
    return NULL;
  }
  t->refs = 1;
  t->arity = arity;
  t->internal = internal;
  t->super = super;
  t->instances = create_instance_map();
  t->name = NULL;
  t->poly = NULL;
  return t;
}

CType *create_poly_var(GType *g, int index) {
  CType *t = allocate(sizeof(CType));
  if (!t) {
    delete_generic(g);
    return NULL;
  }
  t->refs = 1;
  t->type = C_TYPE_POLY_VAR;
  t->internal = INTERNAL_NOTHING;
  t->super = NULL;
  t->name = NULL;
  t->poly_var.type = g;
  t->poly_var.index = index;
  return t;
}

GType *copy_generic(GType *g) {
  if (g) {
    g->refs++;
  }
  return g;
}

void delete_generic(GType *g) {
  if (!g) {
    return;
  }
  g->refs--;
  if (g->refs > 0) {
    return;
  }
  if (g->name) {
    del_ref(SYMBOL(g->name));
  }
  delete_type(g->super);
  // weak reference from g to instance (including poly) => no need to delete
  // each instance
  delete_instance_map(g->instances);
  free(g);
}

CType *copy_type(CType *t) {
  if (t) {
    t->refs++;
  }
  return t;
}

void delete_type(CType *t) {
  if (!t) {
    return;
  }
  t->refs--;
  if (t->refs > 0) {
    return;
  }
  FuncType key;
  switch (t->type) {
    case C_TYPE_SIMPLE:
      delete_type(t->super);
      break;
    case C_TYPE_FUNC:
      key = (FuncType){ .min_arity = t->func.min_arity, .variadic = t->func.variadic };
      free(func_type_map_remove_entry(func_types, &key).key);
      break;
    case C_TYPE_CLOSURE:
      key = (FuncType){ .min_arity = t->func.min_arity, .variadic = t->func.variadic };
      free(func_type_map_remove_entry(closure_types, &key).key);
      break;
    case C_TYPE_INSTANCE:
      instance_map_remove(t->instance.type->instances, (const CType **)t->instance.parameters);
      delete_generic(t->instance.type);
      CType **param = t->instance.parameters;
      while (*param) {
        delete_type(*(param++));
      }
      free(t->instance.parameters);
      break;
    case C_TYPE_POLY_INSTANCE:
      t->poly_instance->poly = NULL;
      delete_generic(t->poly_instance);
      break;
    case C_TYPE_POLY_VAR:
      delete_generic(t->poly_var.type);
      break;
  }
  if (t->name) {
    del_ref(SYMBOL(t->name));
  }
  free(t);
}

Symbol *generic_type_name(const GType *g) {
  return g->name;
}

void set_generic_type_name(GType *g, Symbol *s) {
  if (g->name) {
    del_ref(SYMBOL(g->name));
  }
  if (s) {
    add_ref(SYMBOL(s));
    g->name = s;
  }
}

int generic_type_arity(const GType *g) {
  return g->arity;
}

static void delete_parameters(CType **parameters) {
  while (*parameters) {
    delete_type(*parameters);
  }
}

CType *get_instance(GType *g, CType **parameters) {
  CType *instance = instance_map_lookup(g->instances, (const CType **)parameters);
  if (instance) {
    delete_generic(g);
    return copy_type(instance);
  } else {
    CType **param = parameters;
    int arity = g->arity;
    while (*param) {
      arity--;
      param++;
    }
    if (arity != 0) {
      raise_error(domain_error, "Invalid number of generic parameters, expected %d, got %d", g->arity, g->arity - arity);
      delete_generic(g);
      delete_parameters(parameters);
      return NULL;
    }
    instance = allocate(sizeof(CType));
    if (!instance) {
      delete_generic(g);
      delete_parameters(parameters);
      return NULL;
    }
    CType **param_copy = allocate(sizeof(CType *) * (g->arity + 1));
    if (!param_copy) {
      free(instance);
      delete_generic(g);
      delete_parameters(parameters);
      return NULL;
    }
    param_copy[g->arity] = NULL;
    for (int i = 0; i < g->arity; i++) {
      param_copy[i] = parameters[i];
    }
    instance->refs = 1;
    instance->super = copy_type(g->super);
    instance->type = C_TYPE_INSTANCE;
    instance->internal = g->internal;
    instance->name = NULL;
    instance->instance.type = g;
    instance->instance.parameters = param_copy;
    instance_map_add(g->instances, param_copy, instance);
    return instance;
  }
}

CType *get_unary_instance(GType *g, CType *parameter) {
  return get_instance(move_generic(g), (CType *[]){ parameter, NULL });
}

CType *get_poly_instance(GType *g) {
  if (!g->poly) {
    g->poly = allocate(sizeof(CType));
    if (!g->poly) {
      delete_generic(g);
      return NULL;
    }
    g->poly->refs = 1;
    g->poly->type = C_TYPE_POLY_INSTANCE;
    g->poly->internal = g->internal;
    g->poly->super = copy_type(g->super);
    g->poly->poly_instance = g;
    g->poly->name = NULL;
    return g->poly;
  } else {
    CType *t = copy_type(g->poly);
    delete_generic(g);
    return t;
  }
}

CType *get_func_type(int min_arity, int variadic) {
  FuncType key = (FuncType){ .min_arity = min_arity, .variadic = variadic };
  CType *t = func_type_map_lookup(func_types, &key);
  if (t) {
    return copy_type(t);
  } else {
    t = allocate(sizeof(CType));
    if (!t) {
      return NULL;
    }
    t->refs = 1;
    t->name = NULL;
    t->super = copy_type(func_type);
    t->type = C_TYPE_FUNC;
    t->internal = INTERNAL_FUNC;
    t->func.min_arity = min_arity;
    t->func.variadic = variadic;
    FuncType *key_copy = allocate(sizeof(FuncType));
    if (!key_copy) {
      free(t);
      delete_type(func_type);
      return NULL;
    }
    *key_copy = key;
    func_type_map_add(func_types, key_copy, t);
    return t;
  }
}

CType *get_closure_type(int min_arity, int variadic) {
  FuncType key = (FuncType){ .min_arity = min_arity, .variadic = variadic };
  CType *t = func_type_map_lookup(closure_types, &key);
  if (t) {
    return copy_type(t);
  } else {
    t = allocate(sizeof(CType));
    if (!t) {
      return NULL;
    }
    t->refs = 1;
    t->name = NULL;
    t->super = get_func_type(min_arity, variadic);
    t->type = C_TYPE_CLOSURE;
    t->internal = INTERNAL_CLOSURE;
    t->func.min_arity = min_arity;
    t->func.variadic = variadic;
    FuncType *key_copy = allocate(sizeof(FuncType));
    if (!key_copy) {
      free(t);
      delete_type(func_type);
      return NULL;
    }
    *key_copy = key;
    func_type_map_add(closure_types, key_copy, t);
    return t;
  }
}

CType *instantiate_type(CType *t, const GType *g, CType **parameters) {
  if (t->type == C_TYPE_POLY_VAR && t->poly_var.type == g) {
    if (parameters[t->poly_var.index]) {
      delete_type(t);
      return copy_type(parameters[t->poly_var.index]);
    }
  } else if (t->type == C_TYPE_INSTANCE) {
    GType *t_g = t->instance.type;
    CType **t_param = t->instance.parameters;
    CType **t_param_copy = allocate(sizeof(CType *) * (t_g->arity + 1));
    t_param_copy[t_g->arity] = NULL;
    if (!t_param_copy) {
      delete_type(t);
      return NULL;
    }
    for (int i = 0; i < t_g->arity; i++) {
      t_param_copy[i] = instantiate_type(copy_type(t_param[i]), g, parameters);
      if (!t_param_copy[i]) {
        for (int j = 0; j < i; j++) {
          delete_type(t_param_copy[j]);
        }
        delete_type(t);
        t = NULL;
        break;
      }
    }
    if (t) {
      CType *new_t = get_instance(copy_generic(t_g), t_param_copy);
      delete_type(t);
      t = new_t;
    }
    free(t_param_copy);
  }
  return t;
}

CType *get_super_type(const CType *t) {
  return copy_type(t->super);
}

int is_subtype_of(const CType *a, const CType *b) {
  while (a) {
    if (a == b) {
      return 1;
    } else if (a->type == C_TYPE_POLY_INSTANCE && b->type == C_TYPE_INSTANCE
        && a->poly_instance == b->instance.type) {
      return 1;
    } else if (b->type == C_TYPE_POLY_INSTANCE && a->type == C_TYPE_INSTANCE
        && b->poly_instance == a->instance.type) {
      return 1;
    }
    a = a->super;
  }
  return 0;
}

const CType *unify_types(const CType *a, const CType *b) {
  while (b && a) {
    const CType *tmp = a;
    while (tmp) {
      if (b == tmp) {
        return tmp;
      } else if (tmp->type == C_TYPE_POLY_INSTANCE && b->type == C_TYPE_INSTANCE
          && tmp->poly_instance == b->instance.type) {
        return b;
      } else if (b->type == C_TYPE_POLY_INSTANCE && tmp->type == C_TYPE_INSTANCE
          && b->poly_instance == tmp->instance.type) {
        return tmp;
      }
      tmp = tmp->super;
    }
    b = b->super;
  }
  return any_type;
}

/* Hash function for NULL terminated array of CTypes. */
static Hash c_types_hash(const CType **head) {
  Hash hash = INIT_HASH;
  while (*head) {
    hash = HASH_ADD_PTR(*head, hash);
    head++;
  }
  return hash;
}

/* Equality function for NULL terminated array of CTypes. */
static int c_types_equals(const CType **a, const CType **b) {
  while (*a && *b) {
    if (!*b || !*a || *a != *b) {
      return 0;
    }
    a++;
    b++;
  }
  return 1;
}

/* Hash function for FuncType. */
static Hash func_type_hash(const FuncType *ft) {
  return (ft->min_arity << 1) | ft->variadic;
}

/* Equality function for FuncType. */
static int func_type_equals(const FuncType *a, const FuncType *b) {
  return a == b && a->variadic == b->variadic;
}

DEFINE_HASH_MAP(func_type_map, FuncTypeMap, FuncType *, CType *, func_type_hash, func_type_equals)

DEFINE_HASH_MAP(instance_map, InstanceMap, CType **, CType *, c_types_hash, c_types_equals)

