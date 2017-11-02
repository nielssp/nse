#include <string.h>

#include "module.h"

#include "util/hash_map.h"

DEFINE_PRIVATE_HASH_MAP(namespace, Namespace, char *, NseVal *, string_hash, string_equals)
DEFINE_PRIVATE_HASH_MAP(symmap, SymMap, char *, char *, string_hash, string_equals)
DEFINE_PRIVATE_HASH_MAP(module_map, ModuleMap, char *, Module *, string_hash, string_equals)

struct module {
  const char *name;
  SymMap internal;
  SymMap external;
  Namespace defs;
  Namespace macro_defs;
  Namespace type_defs;
};

static ModuleMap loaded_modules = NULL_HASH_MAP;

void init_modules() {
  //loaded_modules = module_map_create();
  //module_map_add(loaded_modules, "special/modules", ...);
  //module_map_add(loaded_modules, "special/keywords", ...);
}

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
        value = namespace_lookup(scope->module->defs, name);
        break;
      case TYPE_SCOPE:
        value = namespace_lookup(scope->module->type_defs, name);
        break;
    }
    if (value) {
      return *value;
    }
  }
  raise_error("undefined name");
  return undefined;
}

NseVal scope_get_macro(Scope *scope, const char *name) {
  if (scope->module) {
    NseVal *value = namespace_lookup(scope->module->macro_defs, name);
    if (value) {
      return *value;
    }
  }
  raise_error("undefined macro");
  return undefined;
}

Module *create_module(const char *name) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    loaded_modules = create_module_map();
  }
  if (module_map_lookup(loaded_modules, name) != NULL) {
    raise_error("module already defined: %s", name);
    return NULL;
  }
  Module *module = malloc(sizeof(Module));
  module->name = name;
  module->internal = create_symmap();
  module->external = create_symmap();
  module->defs = create_namespace();
  module->macro_defs = create_namespace();
  module->type_defs = create_namespace();
  module_map_add(loaded_modules, name, module);
  return module;
}

static void delete_symbols(SymMap symbols) {
  SymMapIterator it = create_symmap_iterator(symbols);
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    if (entry.value) {
      free(entry.value);
    }
  }
  delete_symmap_iterator(it);
  delete_symmap(symbols);
}

static void delete_names(Namespace namespace) {
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
  delete_symbols(module->internal);
  delete_symbols(module->external);
  delete_names(module->defs);
  delete_names(module->macro_defs);
  delete_names(module->type_defs);
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

Module *find_module(const char *name) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    loaded_modules = create_module_map();
  }
  Module *module = module_map_lookup(loaded_modules, name);
  if (!module) {
    // TODO: attempt to load somehow
  }
  return module;
}

static char *get_symbol_module(const char **s) {
  size_t module_length = 0;
  const char *chars = *s;
  size_t i = 0;
  while (chars[i]) {
    i++;
    if (chars[i] == '/') {
      module_length = i;
    }
  }
  *s += module_length;
  char *module_name = allocate(module_length + 1);
  if (!module_name) {
    return NULL;
  }
  memcpy(module_name, chars, module_length);
  module_name[module_length] = 0;
  return module_name;
}

Symbol *find_symbol(const char *s) {
  char *module_name = get_symbol_module(&s);
  if (module_name) {
    Module *module = find_module(module_name);
    if (module) {
      Symbol *value = symmap_lookup(module->external, s);
      if (value) {
        return value;
      } else {
        raise_error("module %s has no external symbol with name: %s", module_name, s);
      }
    } else {
      raise_error("could not find module: %s", module_name);
    }
    free(module_name);
  }
  return NULL;
}

Symbol *module_intern_symbol(Module *module, const char *s) {
  Symbol *value = symmap_lookup(module->internal, s);
  if (value) {
    return value;
  }
  value = create_symbol(s);
  if (!s) {
    return NULL;
  }
  symmap_add(module->internal, value, value);
  return value;
}

void module_define(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->defs, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->defs, name, copy);
  add_ref(value);
}

void module_define_macro(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->macro_defs, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->macro_defs, name, copy);
  add_ref(value);
}

void module_define_type(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->type_defs, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->type_defs, name, copy);
  add_ref(value);
}

