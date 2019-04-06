#include <string.h>

#include "runtime/value.h"
#include "runtime/error.h"
#include "runtime/hashmap.h"

#include "util/stream.h"

typedef struct Method Method;

DECLARE_HASH_MAP(namespace, Namespace, Symbol *, NseVal *)
DECLARE_HASH_MAP(symmap, SymMap, char *, Symbol *)
DECLARE_HASH_MAP(module_map, ModuleMap, char *, Module *)

DECLARE_HASH_MAP(method_map, MethodMap, Method *, NseVal *)

struct module {
  char *name;
  SymMap internal;
  SymMap external;
  Namespace defs;
  Namespace macro_defs;
  Namespace type_defs;
  Namespace read_macro_defs;
  MethodMap methods;
};

struct Method {
  Symbol *symbol;
  CTypeArray *parameters;
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
        value = namespace_lookup(symbol->module->defs, symbol);
        break;
      case TYPE_SCOPE:
        value = namespace_lookup(symbol->module->type_defs, symbol);
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
    NseVal *value = namespace_lookup(symbol->module->macro_defs, symbol);
    if (value) {
      return *value;
    }
  }
  raise_error(name_error, "undefined macro");
  return undefined;
}

NseVal get_read_macro(Symbol *symbol) {
  if (symbol->module) {
    NseVal *value = namespace_lookup(symbol->module->read_macro_defs, symbol);
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
  module->name = string_copy(name);
  module->internal = create_symmap();
  module->external = create_symmap();
  module->defs = create_namespace();
  module->macro_defs = create_namespace();
  module->type_defs = create_namespace();
  module->read_macro_defs = create_namespace();
  module->methods = create_method_map();
  module_map_add(loaded_modules, module->name, module);
  return module;
}

static void delete_method(Method *method) {
  del_ref(SYMBOL(method->symbol));
  delete_type_array(method->parameters);
  free(method);
}

static void delete_methods(MethodMap methods) {
  MethodMapIterator it = create_method_map_iterator(methods);
  for (MethodMapEntry entry = method_map_next(it); entry.key; entry = method_map_next(it)) {
    delete_method(entry.key);
    del_ref(*entry.value);
    free(entry.value);
  }
  delete_method_map_iterator(it);
  delete_method_map(methods);
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
      del_ref(SYMBOL(entry.key));
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
  delete_methods(module->methods);
  free(module->name);
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

NseVal module_find_method(Module *module, Symbol *symbol, const CTypeArray *parameters) {
  Method query = (Method){ .symbol = symbol, .parameters = (CTypeArray *)parameters };
  NseVal *method = method_map_lookup(module->methods, &query);
  if (method) {
    return *method;
  }
  return undefined;
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
  if (!value) {
    value = create_symbol(s, module);
    if (!s) {
      return NULL;
    }
    symmap_add(module->internal, value->name, value);
  }
  value->refs++;
  return value;
}

NseVal list_external_symbols(Module *module) {
  NseVal tail = nil;
  SymMapIterator it = create_symmap_iterator(module->external);
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    NseVal c = CONS(create_cons(SYMBOL(entry.value), tail));
    del_ref(tail);
    tail = c;
  }
  delete_symmap_iterator(it);
  return tail;
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

int import_method(Module *dest, Symbol *symbol, CTypeArray *parameters, NseVal value) {
  Method *m = allocate(sizeof(Method));
  if (!m) {
    return 0;
  }
  m->symbol = symbol;
  NseVal *value_box = allocate(sizeof(NseVal));
  if (!value_box) {
    free(m);
    return 0;
  }
  *value_box = value;
  m->parameters = parameters;
  if (method_map_add(dest->methods, m, value_box)) {
    add_ref(SYMBOL(symbol));
    copy_type_array(parameters);
    add_ref(value);
  } else {
    free(value_box);
    free(m);
  }
  return 1;
}

int import_methods(Module *dest, Module *src) {
  MethodMapIterator it = create_method_map_iterator(src->methods);
  for (MethodMapEntry entry = method_map_next(it); entry.key; entry = method_map_next(it)) {
    if (!import_method(dest, entry.key->symbol, entry.key->parameters, *entry.value)) {
      delete_method_map_iterator(it);
      return 0;
    }
  }
  delete_method_map_iterator(it);
  return 1;
}

void import_module(Module *dest, Module *src) {
  SymMapIterator it = create_symmap_iterator(src->external);
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    // TODO: detect conflict
    if (symmap_add(dest->internal, entry.value->name, entry.value)) {
      add_ref(SYMBOL(entry.value));
    }
  }
  delete_symmap_iterator(it);
  import_methods(dest, src);
}

void import_module_symbol(Module *dest, Symbol *symbol) {
  symmap_add(dest->internal, symbol->name, symbol);
}

void module_define(Symbol *s, NseVal value) {
  NseVal *existing = namespace_remove(s->module->defs, s);
  if (existing) {
    del_ref(*existing);
    free(existing);
  } else {
    add_ref(SYMBOL(s));
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(s->module->defs, s, copy);
  add_ref(value);
}

void module_define_macro(Symbol *s, NseVal value) {
  NseVal *existing = namespace_remove(s->module->macro_defs, s);
  if (existing) {
    del_ref(*existing);
    free(existing);
  } else {
    add_ref(SYMBOL(s));
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(s->module->macro_defs, s, copy);
  add_ref(value);
}

void module_define_type(Symbol *s, NseVal value) {
  NseVal *existing = namespace_remove(s->module->type_defs, s);
  if (existing) {
    del_ref(*existing);
    free(existing);
  } else {
    add_ref(SYMBOL(s));
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(s->module->type_defs, s, copy);
  add_ref(value);
}

void module_define_read_macro(Symbol *s, NseVal value) {
  NseVal *existing = namespace_remove(s->module->read_macro_defs, s);
  if (existing) {
    del_ref(*existing);
    free(existing);
  } else {
    add_ref(SYMBOL(s));
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  namespace_add(s->module->read_macro_defs, s, copy);
  add_ref(value);
}

void module_define_method(Module *module, Symbol *symbol, CTypeArray *parameters, NseVal value) {
  Method query = (Method){ .symbol = symbol, .parameters = parameters };
  Method *key = NULL;
  MethodMapEntry existing = method_map_remove_entry(module->methods, &query);
  if (existing.key) {
    key = existing.key;
    del_ref(*existing.value);
    free(existing.value);
    delete_type_array(parameters);
  } else {
    key = malloc(sizeof(Method));
    *key = query;
    add_ref(SYMBOL(symbol));
  }
  NseVal *copy = malloc(sizeof(NseVal));
  memcpy(copy, &value, sizeof(NseVal));
  method_map_add(module->methods, key, copy);
  add_ref(value);
}

Symbol *module_ext_define(Module *module, const char *name, NseVal value) {
  Symbol *symbol = module_extern_symbol(module, name);
  module_define(symbol, value);
  return symbol;
}

Symbol *module_ext_define_macro(Module *module, const char *name, NseVal value) {
  Symbol *symbol = module_extern_symbol(module, name);
  module_define_macro(symbol, value);
  return symbol;
}

Symbol *module_ext_define_type(Module *module, const char *name, NseVal value) {
  Symbol *symbol = module_extern_symbol(module, name);
  if (value.type == type_type && value.type_val->name == NULL) {
    value.type_val->name = add_ref(SYMBOL(symbol)).symbol;
  }
  module_define_type(symbol, value);
  return symbol;
}

static size_t method_hash(const Method *m) {
  Hash hash = INIT_HASH;
  hash = HASH_ADD_PTR(m->symbol, hash);
  for (int i = 0; i < m->parameters->size; i++) {
    CType *t = m->parameters->elements[i];
    hash = HASH_ADD_PTR(t, hash);
  }
  return 0;
}

static size_t method_equals(const Method *a, const Method *b) {
  if (a->symbol != b->symbol) {
    return 0;
  }
  if (a->parameters->size != b->parameters->size) {
    return 0;
  }
  for (int i = 0; i < a->parameters->size; i++) {
    if (a->parameters->elements[i] != b->parameters->elements[i]) {
      return 0;
    }
  }
  return 1;
}

DEFINE_HASH_MAP(namespace, Namespace, Symbol *, NseVal *, pointer_hash, pointer_equals)
DEFINE_HASH_MAP(symmap, SymMap, char *, Symbol *, string_hash, string_equals)
DEFINE_HASH_MAP(module_map, ModuleMap, char *, Module *, string_hash, string_equals)
DEFINE_HASH_MAP(method_map, MethodMap, Method *, NseVal *, method_hash, method_equals)
