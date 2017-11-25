#include <string.h>

#include "nsert.h"

#include "util/hash_map.h"
#include "util/stream.h"

DEFINE_PRIVATE_HASH_MAP(namespace, Namespace, char *, NseVal *, string_hash, string_equals)
DEFINE_PRIVATE_HASH_MAP(symmap, SymMap, char *, Symbol *, string_hash, string_equals)
DEFINE_PRIVATE_HASH_MAP(module_map, ModuleMap, char *, Module *, string_hash, string_equals)

struct module {
  const char *name;
  SymMap internal;
  SymMap external;
  Namespace defs;
  Namespace macro_defs;
  Namespace type_defs;
  Namespace read_macro_defs;
};

struct binding {
  size_t refs;
  int weak;
  NseVal value;
};

static ModuleMap loaded_modules = NULL_HASH_MAP;

Module *keyword_module = NULL;

static void init_modules() {
  loaded_modules = create_module_map();
  init_lang_module();
  keyword_module = create_module("keyword");
}

Binding *create_binding(NseVal value) {
  Binding *binding = malloc(sizeof(Binding));
  binding->refs = 1;
  binding->value = add_ref(value);
  binding->weak = 0;
  return  binding;
}

Binding *copy_binding(Binding *binding) {
  binding->refs++;
  return binding;
}

void set_binding(Binding *binding, NseVal value, int weak) {
  NseVal old = binding->value;
  if (!weak) {
    add_ref(value);
  }
  binding->weak = weak;
  binding->value = value;
  del_ref(old);
}

void delete_binding(Binding *binding) {
  if (binding->refs > 1) {
    binding->refs--;
  } else {
    if (!binding->weak) {
      del_ref(binding->value);
    }
    free(binding);
  }
}

Scope *scope_push(Scope *next, Symbol *symbol, NseVal value) {
  Scope *scope = malloc(sizeof(Scope));
  if (symbol) {
    add_ref(SYMBOL(symbol));
  }
  scope->symbol = symbol;
  scope->binding = create_binding(value);
  scope->next = next;
  scope->type = VALUE_SCOPE;
  if (next) {
    scope->module = next->module;
  }
  return scope;
}

Scope *scope_pop(Scope *scope) {
  Scope *next = scope->next;
  if (scope->symbol) {
    del_ref(SYMBOL(scope->symbol));
  }
  delete_binding(scope->binding);
  free(scope);
  return next;
}

void scope_pop_until(Scope *start, Scope *end) {
  while (start != end) {
    Scope *next = start->next;
    if (start->symbol) {
      del_ref(SYMBOL(start->symbol));
    }
    delete_binding(start->binding);
    free(start);
    start = next;
  }
}

Scope *copy_scope(Scope *scope) {
  if (scope == NULL) {
    return NULL;
  }
  Scope *copy = malloc(sizeof(Scope));
  if (scope->symbol) {
    add_ref(SYMBOL(scope->symbol));
  }
  copy->symbol = scope->symbol;
  copy->binding = copy_binding(scope->binding);
  copy->type = scope->type;
  copy->next = copy_scope(scope->next);
  copy->module = scope->module;
  return copy;
}

void delete_scope(Scope *scope) {
  if (scope != NULL) {
    delete_scope(scope->next);
    if (scope->symbol) {
      del_ref(SYMBOL(scope->symbol));
    }
    delete_binding(scope->binding);
    free(scope);
  }
}

int scope_set(Scope *scope, Symbol *symbol, NseVal value, int weak) {
  if (scope->symbol) {
    if (scope->symbol == symbol) {
      set_binding(scope->binding, value, weak);
      return 1;
    }
    if (scope->next) {
      return scope_set(scope->next, symbol, value, weak);
    }
  }
  return 0;
}

NseVal scope_get(Scope *scope, Symbol *symbol) {
  if (scope->symbol) {
    if (scope->symbol == symbol) {
      if (!RESULT_OK(scope->binding->value)) {
        raise_error(name_error, "undefined name: %s", symbol->name);
      }
      return scope->binding->value;
    }
    if (scope->next) {
      return scope_get(scope->next, symbol);
    }
  }
  if (symbol->module) {
    NseVal *value;
    switch (scope->type) {
      case VALUE_SCOPE:
        value = namespace_lookup(symbol->module->defs, symbol->name);
        break;
      case TYPE_SCOPE:
        value = namespace_lookup(symbol->module->type_defs, symbol->name);
        break;
    }
    if (value) {
      return *value;
    }
  }
  raise_error(name_error, "undefined name: %s", symbol->name);
  return undefined;
}

NseVal scope_get_macro(Scope *scope, Symbol *symbol) {
  if (symbol->module) {
    NseVal *value = namespace_lookup(symbol->module->macro_defs, symbol->name);
    if (value) {
      return *value;
    }
  }
  raise_error(name_error, "undefined macro");
  return undefined;
}

NseVal get_read_macro(Symbol *symbol) {
  if (symbol->module) {
    NseVal *value = namespace_lookup(symbol->module->read_macro_defs, symbol->name);
    if (value) {
      return *value;
    }
  }
  raise_error(name_error, "undefined read macro: %s", symbol->name);
  return undefined;
}

Module *create_module(const char *name) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    init_modules();
  }
  if (module_map_lookup(loaded_modules, name) != NULL) {
    raise_error(name_error, "module already defined: %s", name);
    return NULL;
  }
  Module *module = malloc(sizeof(Module));
  if (!module) {
    return NULL;
  }
  module->name = name;
  module->internal = create_symmap();
  module->external = create_symmap();
  module->defs = create_namespace();
  module->macro_defs = create_namespace();
  module->type_defs = create_namespace();
  module->read_macro_defs = create_namespace();
  module_map_add(loaded_modules, name, module);
  return module;
}

static void delete_symbols(SymMap symbols) {
  SymMapIterator it = create_symmap_iterator(symbols);
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    if (entry.value) {
      del_ref(SYMBOL(entry.value));
    }
  }
  delete_symmap_iterator(it);
  delete_symmap(symbols);
}

static void delete_defs(Namespace namespace) {
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
  module_map_remove(loaded_modules, module->name);
  delete_defs(module->defs);
  delete_defs(module->macro_defs);
  delete_defs(module->type_defs);
  delete_defs(module->read_macro_defs);
  delete_symbols(module->internal);
  delete_symbols(module->external);
  free(module);
}

const char *module_name(Module *module) {
  return module->name;
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
    init_modules();
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
  int empty = 1;
  while (chars[i]) {
    i++;
    if (chars[i] == '/') {
      if (!empty) {
        module_length = i;
        empty = 1;
      }
    } else {
      empty = 0;
    }
  }
  *s += module_length + 1;
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
        value->refs++;
        free(module_name);
        return value;
      } else {
        raise_error(name_error, "module %s has no external symbol with name: %s", module_name, s);
      }
    } else {
      raise_error(name_error, "could not find module: %s", module_name);
    }
    free(module_name);
  }
  return NULL;
}

Symbol *module_extern_symbol(Module *module, const char *s) {
  Symbol *value = symmap_lookup(module->external, s);
  if (value) {
    value->refs++;
    return value;
  }
  value = module_intern_symbol(module, s);
  if (!s) {
    return NULL;
  }
  symmap_add(module->external, value->name, value);
  value->refs++;
  return value;
}

Symbol *intern_keyword(const char *s) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    init_modules();
  }
  return module_extern_symbol(keyword_module, s);
}

Symbol *intern_special(const char *s) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    init_modules();
  }
  return module_extern_symbol(lang_module, s);
}

Symbol *module_find_internal(Module *module, const char *s) {
  Symbol *value = symmap_lookup(module->internal, s);
  if (value) {
    value->refs++;
    return value;
  }
  return NULL;
}

Symbol *module_intern_symbol(Module *module, const char *s) {
  Symbol *value = symmap_lookup(module->internal, s);
  if (value) {
    value->refs++;
    return value;
  }
  value = create_symbol(s, module);
  if (!s) {
    return NULL;
  }
  symmap_add(module->internal, value->name, value);
  value->refs++;
  return value;
}

char **get_symbols(Module *module) {
  size_t entries = get_hash_map_size(module->internal.map);
  char **symbols = malloc((entries + 1) * sizeof(char *));
  size_t i = 0;
  symbols[entries] = NULL;
  SymMapIterator it = create_symmap_iterator(module->internal);
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    symbols[i++] = string_printf(entry.value->name);
  }
  delete_symmap_iterator(it);
  return symbols;
}

void import_module(Module *dest, Module *src) {
  SymMapIterator it = create_symmap_iterator(src->external);
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    // TODO: detect conflict
    symmap_add(dest->internal, entry.value->name, entry.value);
  }
  delete_symmap_iterator(it);
}

void import_module_symbol(Module *dest, Symbol *symbol) {
  symmap_add(dest->internal, symbol->name, symbol);
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

void module_define_read_macro(Module *module, const char *name, NseVal value) {
  NseVal *existing = namespace_remove(module->read_macro_defs, name);
  if (existing) {
    del_ref(*existing);
    free(existing);
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(module->read_macro_defs, name, copy);
  add_ref(value);
}

void module_ext_define(Module *module, const char *name, NseVal value) {
  module_define(module, name, value);
  module_extern_symbol(module, name);
}

void module_ext_define_macro(Module *module, const char *name, NseVal value) {
  module_define_macro(module, name, value);
  module_extern_symbol(module, name);
}

void module_ext_define_type(Module *module, const char *name, NseVal value) {
  module_define_type(module, name, value);
  module_extern_symbol(module, name);
}

