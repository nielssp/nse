/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <string.h>

#include "value.h"
#include "type.h"
#include "error.h"
#include "lang.h"
#include "../src/runtime/hashmap.h"
#include "../src/util/stream.h"

#include "module.h"

typedef struct Method Method;

DECLARE_HASH_MAP(namespace, Namespace, Symbol *, Value *)
DECLARE_HASH_MAP(symmap, SymMap, String *, Symbol *)
DECLARE_HASH_MAP(module_map, ModuleMap, String *, Module *)

DECLARE_HASH_MAP(method_map, MethodMap, Method *, Value *)

struct Module {
  String *name;
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
  TypeArray *parameters;
};

struct Binding {
  size_t refs;
  int weak;
  Value value;
};

static ModuleMap loaded_modules = NULL_HASH_MAP;

Module *keyword_module = NULL;

static void init_modules() {
  loaded_modules = create_module_map();
  init_lang_module();
  keyword_module = create_module("keyword");
}

void unload_modules() {
  ModuleMapIterator it = create_module_map_iterator(loaded_modules);
  for (ModuleMapEntry entry = module_map_next(it); entry.key; entry = module_map_next(it)) {
    delete_module(entry.value);
  }
  delete_module_map_iterator(it);
  delete_module_map(loaded_modules);
}

Binding *create_binding(Value value) {
  Binding *binding = malloc(sizeof(Binding));
  binding->refs = 1;
  binding->value = value;
  binding->weak = 0;
  return binding;
}

Binding *copy_binding(Binding *binding) {
  binding->refs++;
  return binding;
}

void set_binding(Binding *binding, Value value, int weak) {
  Value old = binding->value;
  binding->weak = weak;
  binding->value = value;
  delete_value(old);
}

void delete_binding(Binding *binding) {
  if (binding->refs > 1) {
    binding->refs--;
  } else {
    if (!binding->weak) {
      delete_value(binding->value);
    }
    free(binding);
  }
}

Scope *scope_push(Scope *next, Symbol *symbol, Value value) {
  Scope *scope = malloc(sizeof(Scope));
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
    delete_value(SYMBOL(scope->symbol));
  }
  delete_binding(scope->binding);
  free(scope);
  return next;
}

void scope_pop_until(Scope *start, Scope *end) {
  while (start != end) {
    Scope *next = start->next;
    if (start->symbol) {
      delete_value(SYMBOL(start->symbol));
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
    copy_value(SYMBOL(scope->symbol));
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
      delete_value(SYMBOL(scope->symbol));
    }
    delete_binding(scope->binding);
    free(scope);
  }
}

int scope_set(Scope *scope, Symbol *symbol, Value value, int weak) {
  if (scope->symbol) {
    if (scope->symbol == symbol) {
      delete_value(SYMBOL(symbol));
      set_binding(scope->binding, value, weak);
      return 1;
    }
    if (scope->next) {
      return scope_set(scope->next, symbol, value, weak);
    }
  }
  delete_value(SYMBOL(symbol));
  delete_value(value);
  return 0;
}

Value scope_get(Scope *scope, Symbol *symbol) {
  if (scope->symbol) {
    if (scope->symbol == symbol) {
      if (!RESULT_OK(scope->binding->value)) {
        raise_error(name_error, "undefined name: %s", TO_C_STRING(symbol->name));
      }
      delete_value(SYMBOL(symbol));
      return copy_value(scope->binding->value);
    }
    if (scope->next) {
      return scope_get(scope->next, symbol);
    }
  }
  if (symbol->module) {
    Value *value;
    switch (scope->type) {
      case VALUE_SCOPE:
        value = namespace_lookup(symbol->module->defs, symbol);
        break;
      case TYPE_SCOPE:
        value = namespace_lookup(symbol->module->type_defs, symbol);
        break;
    }
    if (value) {
      delete_value(SYMBOL(symbol));
      return copy_value(*value);
    }
  }
  raise_error(name_error, "undefined name: %s", TO_C_STRING(symbol->name));
  delete_value(SYMBOL(symbol));
  return undefined;
}

Value scope_get_macro(Scope *scope, Symbol *symbol) {
  if (symbol->module) {
    Value *value = namespace_lookup(symbol->module->macro_defs, symbol);
    if (value) {
      delete_value(SYMBOL(symbol));
      return copy_value(*value);
    }
  }
  raise_error(name_error, "undefined macro: %s", TO_C_STRING(symbol->name));
  delete_value(SYMBOL(symbol));
  return undefined;
}

Value get_read_macro(Symbol *symbol) {
  if (symbol->module) {
    Value *value = namespace_lookup(symbol->module->read_macro_defs, symbol);
    if (value) {
      delete_value(SYMBOL(symbol));
      return copy_value(*value);
    }
  }
  raise_error(name_error, "undefined read macro: %s", TO_C_STRING(symbol->name));
  delete_value(SYMBOL(symbol));
  return undefined;
}

Module *create_module(const char *name_c) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    init_modules();
  }
  String *name = c_string_to_string(name_c);
  if (module_map_lookup(loaded_modules, name) != NULL) {
    delete_value(STRING(name));
    raise_error(name_error, "module already defined: %s", name);
    return NULL;
  }
  Module *module = malloc(sizeof(Module));
  if (!module) {
    delete_value(STRING(name));
    return NULL;
  }
  module->name = name;
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
  delete_value(SYMBOL(method->symbol));
  delete_type_array(method->parameters);
  free(method);
}

static void delete_methods(MethodMap methods) {
  MethodMapIterator it = create_method_map_iterator(methods);
  for (MethodMapEntry entry = method_map_next(it); entry.key; entry = method_map_next(it)) {
    delete_method(entry.key);
    delete_value(*entry.value);
    free(entry.value);
  }
  delete_method_map_iterator(it);
  delete_method_map(methods);
}

static void delete_symbols(SymMap symbols) {
  SymMapIterator it = create_symmap_iterator(symbols);
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    if (entry.value) {
      delete_value(SYMBOL(entry.value));
    }
  }
  delete_symmap_iterator(it);
  delete_symmap(symbols);
}

static void delete_defs(Namespace namespace) {
  NamespaceIterator it = create_namespace_iterator(namespace);
  for (NamespaceEntry entry = namespace_next(it); entry.key; entry = namespace_next(it)) {
    if (entry.value) {
      delete_value(SYMBOL(entry.key));
      delete_value(*entry.value);
      free(entry.value);
    }
  }
  delete_namespace_iterator(it);
  delete_namespace(namespace);
}

void delete_module(Module *module) {
  delete_defs(module->defs);
  delete_defs(module->macro_defs);
  delete_defs(module->type_defs);
  delete_defs(module->read_macro_defs);
  delete_symbols(module->internal);
  delete_symbols(module->external);
  delete_methods(module->methods);
  delete_value(STRING(module->name));
  free(module);
}

String *get_module_name(Module *module) {
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

Value module_find_method(Module *module, Symbol *symbol, const TypeArray *parameters) {
  Method query = (Method){ .symbol = symbol, .parameters = (TypeArray *)parameters };
  Value *method = method_map_lookup(module->methods, &query);
  if (method) {
    return *method;
  }
  return undefined;
}

Module *find_module(const String *name) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    init_modules();
  }
  Module *module = module_map_lookup(loaded_modules, name);
  if (!module) {
    // TODO: attempt to load somehow
  }
  return module;
}

static int split_symbol_name(const String *s, String **module_name, String **symbol_name) {
  size_t module_length = 0;
  const uint8_t *chars = s->bytes;
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
  *module_name = create_string(chars, module_length);
  if (!*module_name) {
    return 0;
  }
  *symbol_name = create_string(chars + module_length + 1, s->length - module_length - 1);
  if (!*symbol_name) {
    delete_value(STRING(*module_name));
    return 0;
  }
  return 1;
}

Symbol *find_symbol(const String *s) {
  Symbol *result = NULL;
  String *module_name, *symbol_name;
  if (split_symbol_name(s, &module_name, &symbol_name)) {
    Module *module = find_module(module_name);
    if (module) {
      Symbol *value = symmap_lookup(module->external, symbol_name);
      if (value) {
        result = copy_object(value);
      } else {
        raise_error(name_error, "module %s has no external symbol with name: %s", TO_C_STRING(module_name), TO_C_STRING(symbol_name));
      }
    } else {
      raise_error(name_error, "could not find module: %s", TO_C_STRING(module_name));
    }
    delete_value(STRING(module_name));
    delete_value(STRING(symbol_name));
  }
  return result;
}

Symbol *module_extern_symbol(Module *module, String *s) {
  Symbol *value = symmap_lookup(module->external, s);
  if (value) {
    delete_value(STRING(s));
    return copy_object(value);
  }
  value = module_intern_symbol(module, s);
  if (!value) {
    return NULL;
  }
  symmap_add(module->external, value->name, copy_object(value));
  return value;
}

Symbol *module_extern_symbol_c(Module *module, const char *s) {
  return module_extern_symbol(module, c_string_to_string(s));
}

Symbol *intern_keyword(String *s) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    init_modules();
  }
  return module_extern_symbol(keyword_module, s);
}

Symbol *intern_special(String *s) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    init_modules();
  }
  return module_extern_symbol(lang_module, s);
}

Symbol *module_find_internal(Module *module, const String *s) {
  Symbol *value = symmap_lookup(module->internal, s);
  if (value) {
    return copy_object(value);
  }
  return NULL;
}

Symbol *module_intern_symbol(Module *module, String *s) {
  Symbol *value = symmap_lookup(module->internal, s);
  if (value) {
    delete_value(STRING(s));
  } else {
    value = create_symbol(s, module);
    if (!value) {
      return NULL;
    }
    symmap_add(module->internal, value->name, value);
  }
  return copy_object(value);
}

Value list_external_symbols(Module *module) {
  Vector *v = create_vector(get_hash_map_size(module->external.map));
  if (!v) {
    return undefined;
  }
  SymMapIterator it = create_symmap_iterator(module->external);
  size_t i = 0;
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    v->cells[i++] = copy_value(SYMBOL(entry.value));
  }
  delete_symmap_iterator(it);
  return VECTOR(v);
}

char **get_symbols(Module *module) {
  size_t entries = get_hash_map_size(module->internal.map);
  char **symbols = malloc((entries + 1) * sizeof(char *));
  size_t i = 0;
  symbols[entries] = NULL;
  SymMapIterator it = create_symmap_iterator(module->internal);
  for (SymMapEntry entry = symmap_next(it); entry.key; entry = symmap_next(it)) {
    symbols[i++] = string_printf(TO_C_STRING(entry.value->name));
  }
  delete_symmap_iterator(it);
  return symbols;
}

static int import_method(Module *dest, Symbol *symbol, TypeArray *parameters, Value value) {
  Method *m = allocate(sizeof(Method));
  if (!m) {
    return 0;
  }
  m->symbol = symbol;
  Value *value_box = allocate(sizeof(Value));
  if (!value_box) {
    free(m);
    return 0;
  }
  *value_box = value;
  m->parameters = parameters;
  if (method_map_add(dest->methods, m, value_box)) {
    copy_value(SYMBOL(symbol));
    copy_type_array(parameters);
    copy_value(value);
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
      copy_value(SYMBOL(entry.value));
    }
  }
  delete_symmap_iterator(it);
  import_methods(dest, src);
}

void module_define(Symbol *s, Value value) {
  Value *existing = namespace_remove(s->module->defs, s);
  if (existing) {
    delete_value(SYMBOL(s));
    delete_value(*existing);
    free(existing);
  }
  Value *copy = malloc(sizeof(Value));
  memcpy(copy, &value, sizeof(Value));
  namespace_add(s->module->defs, s, copy);
}

void module_define_macro(Symbol *s, Value value) {
  Value *existing = namespace_remove(s->module->macro_defs, s);
  if (existing) {
    delete_value(SYMBOL(s));
    delete_value(*existing);
    free(existing);
  }
  Value *copy = malloc(sizeof(Value));
  memcpy(copy, &value, sizeof(Value));
  namespace_add(s->module->macro_defs, s, copy);
}

void module_define_type(Symbol *s, Value value) {
  Value *existing = namespace_remove(s->module->type_defs, s);
  if (existing) {
    delete_value(SYMBOL(s));
    delete_value(*existing);
    free(existing);
  }
  Value *copy = malloc(sizeof(Value));
  memcpy(copy, &value, sizeof(Value));
  namespace_add(s->module->type_defs, s, copy);
}

void module_define_read_macro(Symbol *s, Value value) {
  Value *existing = namespace_remove(s->module->read_macro_defs, s);
  if (existing) {
    delete_value(SYMBOL(s));
    delete_value(*existing);
    free(existing);
  }
  Value *copy = malloc(sizeof(Value));
  memcpy(copy, &value, sizeof(Value));
  namespace_add(s->module->read_macro_defs, s, copy);
}

void module_define_method(Module *module, Symbol *symbol, TypeArray *parameters, Value value) {
  Method query = (Method){ .symbol = symbol, .parameters = parameters };
  Method *key = NULL;
  MethodMapEntry existing = method_map_remove_entry(module->methods, &query);
  if (existing.key) {
    key = existing.key;
    delete_value(*existing.value);
    free(existing.value);
    delete_value(SYMBOL(symbol));
    delete_type_array(parameters);
  } else {
    key = malloc(sizeof(Method));
    *key = query;
    copy_value(SYMBOL(symbol));
  }
  Value *copy = malloc(sizeof(Value));
  memcpy(copy, &value, sizeof(Value));
  method_map_add(module->methods, key, copy);
}

Symbol *module_ext_define(Module *module, const char *name, Value value) {
  Symbol *symbol = module_extern_symbol(module, c_string_to_string(name));
  module_define(symbol, value);
  return symbol;
}

Symbol *module_ext_define_macro(Module *module, const char *name, Value value) {
  Symbol *symbol = module_extern_symbol(module, c_string_to_string(name));
  module_define_macro(symbol, value);
  return symbol;
}

Symbol *module_ext_define_type(Module *module, const char *name, Value value) {
  Symbol *symbol = module_extern_symbol(module, c_string_to_string(name));
  if (value.type == VALUE_TYPE && TO_TYPE(value)->name == NULL) {
    TO_TYPE(value)->name = copy_object(symbol);
  }
  module_define_type(symbol, value);
  return symbol;
}

static Hash method_hash(const Method *m) {
  Hash hash = INIT_HASH;
  hash = HASH_ADD_PTR(m->symbol, hash);
  for (int i = 0; i < m->parameters->size; i++) {
    Type *t = m->parameters->elements[i];
    hash = HASH_ADD_PTR(t, hash);
  }
  return 0;
}

static int method_equals(const Method *a, const Method *b) {
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

static Hash nse_string_hash(const String *s) {
  return string_hash(TO_C_STRING(s));
}

static int nse_string_equals(const String *a, const String *b) {
  return string_equals(TO_C_STRING(a), TO_C_STRING(b));
}

DEFINE_HASH_MAP(namespace, Namespace, Symbol *, Value *, pointer_hash, pointer_equals)
DEFINE_HASH_MAP(symmap, SymMap, String *, Symbol *, nse_string_hash, nse_string_equals)
DEFINE_HASH_MAP(module_map, ModuleMap, String *, Module *, nse_string_hash, nse_string_equals)
DEFINE_HASH_MAP(method_map, MethodMap, Method *, Value *, method_hash, method_equals)
