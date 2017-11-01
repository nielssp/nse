#include "module.h"


DEFINE_HASH_MAP(namespace, Namespace, char *, NseVal *, string_hash, string_equals)
DEFINE_PRIVATE_HASH_MAP(symmap, SymMap, char *, char *, string_hash, string_equals)
DEFINE_PRIVATE_HASH_MAP(module_map, ModuleMap, char *, Module *, string_hash, string_equals)

struct module {
  const char *name;
  SymMap internal;
  SymMap external;
  Namespace definitions;
  Namespace macro_definitions;
  Namespace type_definitions;
};

typedef enum {
  TYPE_SCOPE,
  VALUE_SCOPE
} ScopeType;

struct scope {
  Module *module;
  const char *name;
  NseVal value;
  Scope *next;
  ScopeType type;
};

static ModuleMap *loaded_modules = NULL;

Scope *scope_push(Scope *next, const char *name, NseVal value) {
  Scope *scope = malloc(sizeof(Scope));
  scope->name = name;
  scope->value = value;
  scope->next = next;
  scope->type = VALUE_SCOPE;
  if (next) {
    scope->module = next->module;
  }
  add_ref(value);
  return scope;
}

Scope *scope_pop(Scope *scope) {
  Scope *next = scope->next;
  del_ref(scope->value);
  free(scope);
  return next;
}

void scope_pop_until(Scope *start, Scope *end) {
  while (start != end) {
    Scope *next = start->next;
    del_ref(start->value);
    free(start);
    start = next;
  }
}

Scope *copy_scope(Scope *scope) {
  if (scope == NULL) {
    return NULL;
  }
  Scope *copy = malloc(sizeof(Scope));
  copy->name = scope->name;
  copy->value = scope->value;
  copy->type = scope->type;
  copy->next = copy_scope(scope->next);
  copy->module = scope->module;
  add_ref(copy->value);
  return copy;
}

void delete_scope(Scope *scope) {
  if (scope != NULL) {
    delete_scope(scope->next);
    del_ref(scope->value);
    free(scope);
  }
}

NseVal scope_get(Scope *scope, const char *name) {
  if (scope->name) {
    if (strcmp(name, scope->name) == 0) {
      return scope->value;
    }
    if (scope->next) {
      return scope_get(scope->next, name);
    }
  }
  if (scope->module) {
    NseVal *value;
    switch (scope->type) {
      case VALUE_SCOPE:
        value = namespace_lookup(scope->module->internal, name);
        break;
      case TYPE_SCOPE:
        value = namespace_lookup(scope->module->internal_types, name);
        break;
    }
    if (value) {
      return *value;
    }
  }
  raise_error("undefined name");
  return undefined;
}

Module *create_module(const char *name) {
  Module *module = malloc(sizeof(Module));
  module->name = name;
  module->internal = create_symmap();
  module->external = create_symmap();
  module->definitions = create_namespace();
  module->macro_definitions = create_namespace();
  module->type_definitions = create_namespace();
  return module;
}

void delete_names(Namespace namespace) {
  NamespaceIterator it = create_namespace_iterator(namespace);
  for (NamespaceEntry entry = namespace_next(it); entry.key; entry = namespace_next(it)) {
    if (entry.value) {
      del_ref(*entry.value);
      free(entry.value);
    }
  }
  delete_namespace_iterator(it);
  delete_namespace(namespace);
}

void delete_module(Module *module) {
  delete_names(module->internal);
  delete_names(module->internal_macros);
  delete_names(module->internal_types);
  free(module);
}

Scope *use_module(Module *module) {
  Scope *scope = scope_push(NULL, NULL, undefined);
  scope->module = module;
  return scope;
}

Scope *use_module_types(Module *module) {
  Scope *scope = scope_push(NULL, NULL, undefined);
  scope->module = module;
  scope->type = TYPE_SCOPE;
  return scope;
}

void module_define(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->internal, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->internal, name, copy);
  add_ref(value);
}

void module_define_macro(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->internal_macros, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->internal_macros, name, copy);
  add_ref(value);
}

void module_define_type(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->internal_types, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->internal_types, name, copy);
  add_ref(value);
}

