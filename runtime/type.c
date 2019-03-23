#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "hashmap.h"
#include "value.h"

#include "type.h"

size_t c_types_hash(const void *p) {
  CType **head = (CType **)p;
  size_t hash = 0;
  while (*head) {
    hash ^= (size_t)*head;
    head++;
  }
  return hash;
}

int c_types_equals(const void *a, const void *b) {
  CType **head_a = (CType **)a;
  CType **head_b = (CType **)b;
  while (*head_a && *head_b) {
    if (!*head_b || !*head_a || *head_a != *head_b) {
      return 0;
    }
    head_a++;
    head_b++;
  }
  return 1;
}

DEFINE_PRIVATE_HASH_MAP(instance_map, InstanceMap, CType **, CType *, c_types_hash, c_types_equals)

typedef struct {
  int min_arity;
  int variadic;
} FuncType;

size_t func_type_hash(const void *p) {
  FuncType *ft = (FuncType *)p;
  return (ft->min_arity << 1) | ft->variadic;
}

int func_type_equals(const void *a, const void *b) {
  FuncType *ft_a = (FuncType *)a;
  FuncType *ft_b = (FuncType *)b;
  return ft_a->min_arity == ft_b->min_arity && ft_a->variadic == ft_b->variadic;
}

DEFINE_PRIVATE_HASH_MAP(func_type_map, FuncTypeMap, FuncType *, CType *, func_type_hash, func_type_equals)

struct GType {
  size_t refs;
  int arity;
  Symbol *name;
  InternalType internal;
  CType *super;
  InstanceMap instances;
};

FuncTypeMap func_types;
FuncTypeMap closure_types;

CType *any_type;
CType *improper_list_type;
CType *any_list_type;
CType *nil_type;
CType *cons_type;
CType *num_type;
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

GType *list_type;

void init_types() {
  func_types = create_func_type_map();
  closure_types = create_func_type_map();
  any_type = create_simple_type(INTERNAL_NOTHING, NULL);
  improper_list_type = create_simple_type(INTERNAL_NOTHING, any_type);
  cons_type = create_simple_type(INTERNAL_CONS, improper_list_type);
  list_type = create_generic(1, INTERNAL_CONS, improper_list_type);
  any_list_type = get_instance(list_type, (CType *[]){ copy_type(any_type), NULL });
  nil_type = create_simple_type(INTERNAL_NIL, any_list_type);
  num_type = create_simple_type(INTERNAL_NOTHING, any_type);
  i64_type = create_simple_type(INTERNAL_I64, num_type);
  f64_type = create_simple_type(INTERNAL_F64, num_type);
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
}

CType *create_simple_type(InternalType internal, CType *super) {
  CType *t = allocate(sizeof(CType));
  t->refs = 1;
  t->type = C_TYPE_SIMPLE;
  t->internal = internal;
  t->super = super;
  t->name = NULL;
  return t;
}

GType *create_generic(int arity, InternalType internal, CType *super) {
  GType *t = allocate(sizeof(GType));
  t->refs = 1;
  t->arity = arity;
  t->internal = internal;
  t->super = super;
  t->instances = create_instance_map();
  t->name = NULL;
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
  delete_type(g->super);
  // TODO: delete instances
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
  switch (t->type) {
    case C_TYPE_SIMPLE:
      delete_type(t->super);
      break;
    case C_TYPE_FUNC:
    case C_TYPE_CLOSURE:
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
  }
  free(t);
}

Symbol *generic_type_name(GType *g) {
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

CType *get_instance(GType *g, CType **parameters) {
  CType *instance = instance_map_lookup(g->instances, (const CType **)parameters);
  if (instance) {
    return copy_type(instance);
  } else {
    CType **param = parameters;
    CType *super = copy_type(any_type);
    int arity = g->arity;
    while (*param) {
      arity--;
      if (*param != any_type && super) {
        delete_type(super);
        super = NULL;
      }
      param++;
    }
    if (arity != 0) {
      raise_error(domain_error, "Invalid number of generic parameters, expected %d, got %d", g->arity, g->arity - arity);
      return NULL;
    }
    CType **param_copy = allocate(sizeof(CType *) * (g->arity + 1));
    param_copy[g->arity] = NULL;
    for (int i = 0; i < g->arity; i++) {
      param_copy[i] = copy_type(parameters[i]);
    }
    if (!super) {
      CType **param_copy2 = allocate(sizeof(CType *) * (g->arity + 1));
      param_copy2[g->arity] = NULL;
      for (int i = 0; i < g->arity; i++) {
        param_copy2[i] = any_type;
      }
      super = get_instance(g, param_copy2);
      free(param_copy2);
    }
    instance = allocate(sizeof(CType));
    instance->refs = 1;
    instance->super = super;
    instance->type = C_TYPE_INSTANCE;
    instance->internal = g->internal;
    instance->instance.type = g;
    instance->instance.parameters = param_copy;
    instance_map_add(g->instances, param_copy, instance);
    return instance;
  }
}

CType *get_unary_instance(GType *g, CType *parameter) {
  return get_instance(g, (CType *[]){ parameter, NULL });
}

CType *get_func_type(int min_arity, int variadic) {
  FuncType key = (FuncType){ .min_arity = min_arity, .variadic = variadic };
  CType *t = func_type_map_lookup(func_types, &key);
  if (t) {
    return copy_type(t);
  } else {
    t = allocate(sizeof(CType));
    t->refs = 1;
    t->super = copy_type(func_type);
    t->type = C_TYPE_FUNC;
    t->internal = INTERNAL_FUNC;
    t->func.min_arity = min_arity;
    t->func.variadic = variadic;
    FuncType *key_copy = allocate(sizeof(FuncType));
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
    t->refs = 1;
    t->super = get_func_type(min_arity, variadic);
    t->type = C_TYPE_CLOSURE;
    t->internal = INTERNAL_CLOSURE;
    t->func.min_arity = min_arity;
    t->func.variadic = variadic;
    FuncType *key_copy = allocate(sizeof(FuncType));
    *key_copy = key;
    func_type_map_add(closure_types, key_copy, t);
    return t;
  }
}

CType *get_super_type(CType *t) {
  return copy_type(t->super);
}


