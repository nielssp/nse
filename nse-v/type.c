/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "hashmap.h"
#include "value.h"

#include "type.h"

/* Structure used as a key in function type hash maps. */
typedef struct {
  int min_arity;
  int variadic;
} FuncType;

DECLARE_HASH_MAP(instance_map, InstanceMap, TypeArray *, Type *)
DECLARE_HASH_MAP(func_type_map, FuncTypeMap, FuncType, Type *)

/* A generic type. */
struct GType {
  size_t refs;
  int arity;
  /* Optional name. */
  Symbol *name;
  /* Optional super type. */
  Type *super;
  /* Hash map of instances. Instances are weakly referenced. When an instance is
   * deleted (refs=0) it is automatically removed from the instance map of the
   * corresponding generic type by `delete_type()`. */
  InstanceMap instances;
  /* Optional weak reference to polymorphic instance. When the instance is
   * deleted (i.e. `poly->refs == 0`), the field is automatically set to NULL by
   * `delete_type()`. */
  Type *poly;
};

/* Map of function type instances. */
FuncTypeMap func_types;

Type *nothing_type;
Type *any_type;
Type *unit_type;
Type *bool_type;
Type *num_type;
Type *int_type;
Type *float_type;
Type *i64_type;
Type *f64_type;
Type *string_type;
Type *symbol_type;
Type *keyword_type;
Type *continue_type;
Type *syntax_type;
Type *type_type;
Type *func_type;
Type *scope_type;
Type *stream_type;
Type *generic_type_type;

GType *result_type;
GType *vector_type;
GType *vector_slice_type;
GType *list_type;
GType *weak_ref_type;
GType *hash_map_type;
GType *entry_type;

void init_types(void) {
  init_func_type_map(&func_types);
  nothing_type = create_simple_type(NULL);
  any_type = create_simple_type(NULL);
  unit_type = create_simple_type(copy_type(any_type));
  bool_type = create_simple_type(copy_type(any_type));
  num_type = create_simple_type(copy_type(any_type));
  int_type = create_simple_type(copy_type(num_type));
  float_type = create_simple_type(copy_type(num_type));
  i64_type = create_simple_type(copy_type(int_type));
  f64_type = create_simple_type(copy_type(float_type));
  string_type = create_simple_type(copy_type(any_type));
  symbol_type = create_simple_type(copy_type(any_type));
  keyword_type = create_simple_type(copy_type(any_type));
  continue_type = create_simple_type(copy_type(any_type));
  syntax_type = create_simple_type(copy_type(any_type));
  type_type = create_simple_type(copy_type(any_type));
  func_type = create_simple_type(copy_type(any_type));
  scope_type = create_simple_type(copy_type(any_type));
  stream_type = create_simple_type(copy_type(any_type));
  generic_type_type = create_simple_type(copy_type(any_type));

  result_type = create_generic(2, copy_type(any_type));
  vector_type = create_generic(1, copy_type(any_type));
  vector_slice_type = create_generic(1, copy_type(any_type));
  list_type = create_generic(1, copy_type(any_type));
  weak_ref_type = create_generic(1, copy_type(any_type));
  hash_map_type = create_generic(2, copy_type(any_type));
  entry_type = create_generic(2, copy_type(any_type));
}

Type *create_simple_type(Type *super) {
  Type *t = allocate_object(sizeof(Type));
  if (!t) {
    delete_type(super);
    return NULL;
  }
  t->type = TYPE_SIMPLE;
  t->super = super;
  t->name = NULL;
  return t;
}

GType *create_generic(int arity, Type *super) {
  GType *t = allocate(sizeof(GType));
  if (!t) {
    delete_type(super);
    return NULL;
  }
  t->refs = 1;
  t->arity = arity;
  t->super = super;
  if (!init_instance_map(&t->instances)) {
    delete_type(super);
    free(t);
    raise_error(out_of_memory_error, "could not allocate memory");
    return NULL;
  }
  t->name = NULL;
  t->poly = NULL;
  return t;
}

Type *create_poly_var(GType *g, int index) {
  Type *t = allocate_object(sizeof(Type));
  if (!t) {
    delete_generic(g);
    return NULL;
  }
  t->type = TYPE_POLY_VAR;
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
    delete_value(SYMBOL(g->name));
  }
  delete_type(g->super);
  // weak reference from g to instance (including poly) => no need to delete
  // each instance
  delete_instance_map(&g->instances);
  free(g);
}

Type *copy_type(Type *t) {
  if (t) {
    t->header.refs++;
  }
  return t;
}

void delete_type(Type *t) {
  if (!t) {
    return;
  }
  t->header.refs--;
  if (t->header.refs > 0) {
    return;
  }
  FuncType key;
  switch (t->type) {
    case TYPE_SIMPLE:
      delete_type(t->super);
      break;
    case TYPE_FUNC:
      key = (FuncType){ .min_arity = t->func.min_arity, .variadic = t->func.variadic };
      func_type_map_remove_entry(&func_types, key, NULL);
      break;
    case TYPE_INSTANCE:
      instance_map_remove(&t->instance.type->instances, t->instance.parameters, NULL);
      delete_generic(t->instance.type);
      delete_type_array(t->instance.parameters);
      break;
    case TYPE_POLY_INSTANCE:
      t->poly_instance->poly = NULL;
      delete_generic(t->poly_instance);
      break;
    case TYPE_POLY_VAR:
      delete_generic(t->poly_var.type);
      break;
  }
  if (t->name) {
    delete_value(SYMBOL(t->name));
  }
  free(t);
}

TypeArray *create_type_array(size_t size, Type * const elements[]) {
  TypeArray *a = allocate(sizeof(TypeArray) + sizeof(Type *) * size);
  if (!a) {
    for (int i = 0; i < size; i++) {
      delete_type(elements[i]);
    }
    return NULL;
  }
  a->refs = 1;
  a->size = size;
  memcpy(a->elements, elements, sizeof(Type *) * size);
  return a;
}

TypeArray *create_type_array_null(size_t size) {
  TypeArray *a = allocate(sizeof(TypeArray) + sizeof(Type *) * size);
  if (!a) {
    return NULL;
  }
  a->refs = 1;
  a->size = size;
  for (int i = 0; i < size; i++) {
    a->elements[i] = NULL;
  }
  return a;
}

TypeArray *copy_type_array(TypeArray *a) {
  if (a) {
    a->refs++;
  }
  return a;
}

void delete_type_array(TypeArray *a) {
  if (!a) {
    return;
  }
  a->refs--;
  if (a->refs > 0) {
    return;
  }
  for (int i = 0; i < a->size; i++) {
    delete_type(a->elements[i]);
  }
  free(a);
}

Symbol *generic_type_name(const GType *g) {
  return g->name;
}

void set_generic_type_name(GType *g, Symbol *s) {
  if (g->name) {
    delete_value(SYMBOL(g->name));
  }
  if (s) {
    g->name = s;
  }
}

int generic_type_arity(const GType *g) {
  return g->arity;
}

Type *get_instance(GType *g, TypeArray *parameters) {
  Type *instance;
  if (instance_map_get(&g->instances, parameters, &instance)) {
    delete_generic(g);
    delete_type_array(parameters);
    return copy_type(instance);
  } else {
    if (g->arity != parameters->size) {
      raise_error(domain_error, "Invalid number of generic parameters, expected %d, got %d", g->arity, parameters->size);
      delete_generic(g);
      delete_type_array(parameters);
      return NULL;
    }
    instance = allocate_object(sizeof(Type));
    if (!instance) {
      delete_generic(g);
      delete_type_array(parameters);
      return NULL;
    }
    instance->super = copy_type(g->super);
    instance->type = TYPE_INSTANCE;
    instance->name = NULL;
    instance->instance.type = g;
    instance->instance.parameters = parameters;
    instance_map_add(&g->instances, parameters, instance);
    return instance;
  }
}

Type *get_unary_instance(GType *g, Type *parameter) {
  TypeArray *a = create_type_array(1, (Type *[]){ move_type(parameter) });
  return get_instance(move_generic(g), move_type_array(a));
}

Type *get_poly_instance(GType *g) {
  if (!g->poly) {
    g->poly = allocate_object(sizeof(Type));
    if (!g->poly) {
      delete_generic(g);
      return NULL;
    }
    g->poly->type = TYPE_POLY_INSTANCE;
    g->poly->super = copy_type(g->super);
    g->poly->poly_instance = g;
    g->poly->name = NULL;
    return g->poly;
  } else {
    Type *t = copy_type(g->poly);
    delete_generic(g);
    return t;
  }
}

static Type *get_func_subtype(int min_arity, int variadic, FuncTypeMap *map, TypeType type) {
  FuncType key = (FuncType){ .min_arity = min_arity, .variadic = variadic };
  Type *t;
  if (func_type_map_get(map, key, &t)) {
    return copy_type(t);
  } else {
    t = allocate_object(sizeof(Type));
    if (!t) {
      return NULL;
    }
    t->name = NULL;
    if (type == TYPE_FUNC) {
      t->super = copy_type(func_type);
    } else {
      t->super = get_func_type(min_arity, variadic);
    }
    t->type = type;
    t->func.min_arity = min_arity;
    t->func.variadic = variadic;
    func_type_map_add(map, key, t);
    return t;
  }
}

Type *get_func_type(int min_arity, int variadic) {
  return get_func_subtype(min_arity, variadic, &func_types, TYPE_FUNC);
}

Type *instantiate_type(Type *t, const GType *g, const TypeArray *parameters) {
  if (t->type == TYPE_POLY_VAR && t->poly_var.type == g) {
    if (parameters->elements[t->poly_var.index]) {
      delete_type(t);
      return copy_type(parameters->elements[t->poly_var.index]);
    }
  } else if (t->type == TYPE_INSTANCE) {
    GType *t_g = t->instance.type;
    TypeArray *t_param = t->instance.parameters;
    TypeArray *t_param_copy = create_type_array_null(t_param->size);
    if (!t_param_copy) {
      delete_type(t);
      return NULL;
    }
    for (int i = 0; i < t_g->arity; i++) {
      t_param_copy->elements[i] = instantiate_type(copy_type(t_param->elements[i]), g, parameters);
      if (!t_param_copy->elements[i]) {
        for (int j = 0; j < i; j++) {
          delete_type(t_param_copy->elements[j]);
          t_param_copy->elements[j] = NULL;
        }
        delete_type(t);
        t = NULL;
        break;
      }
    }
    if (t) {
      Type *new_t = get_instance(copy_generic(t_g), t_param_copy);
      delete_type(t);
      t = new_t;
    } else {
      delete_type_array(t_param_copy);
    }
  }
  return t;
}

Type *get_super_type(const Type *t) {
  return copy_type(t->super);
}

int is_subtype_of(const Type *a, const Type *b) {
  while (a) {
    if (a == b) {
      return 1;
    } else if (a->type == TYPE_POLY_INSTANCE && b->type == TYPE_INSTANCE
        && a->poly_instance == b->instance.type) {
      return 1;
    } else if (b->type == TYPE_POLY_INSTANCE && a->type == TYPE_INSTANCE
        && b->poly_instance == a->instance.type) {
      return 1;
    }
    a = a->super;
  }
  return 0;
}

int are_subtypes_of(const TypeArray *a, const TypeArray *b) {
  if (a == b) {
    return 1;
  }
  if (a->size != b->size) {
    return 0;
  }
  for (size_t i = 0; i < a->size; i++) {
    if (!is_subtype_of(a->elements[i], b->elements[i])) {
      return 0;
    }
  }
  return 1;
}

Type *unify_types(Type *a, Type *b) {
  Type *result = NULL;
  Type *tmp1 = b;
  while (tmp1) {
    Type *tmp2 = a;
    while (tmp2) {
      if (tmp1 == tmp2) {
        result = copy_type(tmp2);
        break;
      } else if (tmp2->type == TYPE_POLY_INSTANCE && tmp1->type == TYPE_INSTANCE
          && tmp2->poly_instance == tmp1->instance.type) {
        result = copy_type(tmp1);
        break;
      } else if (tmp1->type == TYPE_POLY_INSTANCE && tmp2->type == TYPE_INSTANCE
          && tmp1->poly_instance == tmp2->instance.type) {
        result = copy_type(tmp2);
        break;
      }
      tmp2 = tmp2->super;
    }
    if (result) {
      break;
    }
    tmp1 = tmp1->super;
  }
  delete_type(a);
  delete_type(b);
  if (!result) {
    return copy_type(any_type);
  }
  return result;
}

Type *get_type(const Value value) {
  switch (value.type) {
    case VALUE_UNDEFINED:
      return NULL;
    case VALUE_UNIT:
      return copy_type(unit_type);
    case VALUE_I64:
      return copy_type(i64_type);
    case VALUE_F64:
      return copy_type(f64_type);
    case VALUE_FUNC:
      return copy_type(func_type);

    case VALUE_VECTOR:
      if (!TO_VECTOR(value)->type) {
        TO_VECTOR(value)->type = get_unary_instance(copy_generic(vector_type),
            copy_type(any_type));
      }
      return copy_type(TO_VECTOR(value)->type);
    case VALUE_VECTOR_SLICE:
      return get_type(VECTOR(TO_VECTOR_SLICE(value)->vector));
    case VALUE_ARRAY:
    case VALUE_ARRAY_SLICE:
      return NULL;
    case VALUE_LIST:
      return get_unary_instance(copy_generic(list_type), copy_type(any_type));
    case VALUE_STRING:
      return copy_type(string_type);
    case VALUE_WEAK_REF:
      if (!TO_WEAK_REF(value)->type) {
        TO_WEAK_REF(value)->type = get_unary_instance(copy_generic(weak_ref_type),
            copy_type(any_type));
      }
      return copy_type(TO_WEAK_REF(value)->type);
    case VALUE_SYMBOL:
      return copy_type(symbol_type);
    case VALUE_KEYWORD:
      return copy_type(keyword_type);
    case VALUE_DATA:
      return copy_type(TO_DATA(value)->type);
    case VALUE_SYNTAX:
      return copy_type(syntax_type);
    case VALUE_CLOSURE:
      return copy_type(func_type);
    case VALUE_POINTER:
      return copy_type(TO_POINTER(value)->type);
    case VALUE_TYPE:
      return copy_type(type_type);
    case VALUE_GEN_FUNC:
      return copy_type(func_type);
    case VALUE_HASH_MAP:
      if (!TO_HASH_MAP(value)->type) {
        TypeArray *a = create_type_array(2, (Type *[]){ copy_type(any_type), copy_type(any_type) });
        TO_HASH_MAP(value)->type = get_instance(copy_generic(hash_map_type), a);
      }
      return copy_type(TO_HASH_MAP(value)->type);
  }
}

/* Hash function for type arrays. */
static Hash type_array_hash(const TypeArray *a) {
  Hash hash = INIT_HASH;
  for (int i = 0; i < a->size; i++) {
    hash = HASH_ADD_PTR(a->elements[i], hash);
  }
  return hash;
}

int type_array_equals(const TypeArray *a, const TypeArray *b) {
  if (a == b) {
    return 1;
  }
  if (a->size != b->size) {
    return 0;
  }
  for (int i = 0; i < a->size; i++) {
    if (a->elements[i] != b->elements[i]) {
      return 0;
    }
  }
  return 1;
}

/* Hash function for FuncType. */
static Hash func_type_hash(const FuncType ft) {
  return (ft.min_arity << 1) | ft.variadic;
}

/* Equality function for FuncType. */
static int func_type_equals(const FuncType a, const FuncType b) {
  return a.min_arity == b.min_arity && a.variadic == b.variadic;
}

DEFINE_HASH_MAP(func_type_map, FuncTypeMap, FuncType, Type *, func_type_hash, func_type_equals)

DEFINE_HASH_MAP(instance_map, InstanceMap, TypeArray *, Type *, type_array_hash, type_array_equals)

