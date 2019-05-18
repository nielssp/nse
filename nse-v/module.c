/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <string.h>
#include <stdarg.h>

#include "value.h"
#include "type.h"
#include "error.h"
#include "lang.h"
#include "hashmap.h"
#include "../src/util/stream.h"

#include "module.h"

typedef struct MethodKey MethodKey;
typedef struct MethodList MethodList;

DECLARE_HASH_MAP(namespace, Namespace, Symbol *, Value)
DECLARE_HASH_MAP(symmap, SymMap, const String *, Symbol *)
DECLARE_HASH_MAP(module_map, ModuleMap, const String *, Module *)

struct MethodKey {
  Symbol *symbol;
  Type *type;
};

DECLARE_HASH_MAP(method_map, MethodMap, MethodKey, MethodList *)

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

struct MethodList {
  TypeArray *parameters;
  Value definition;
  MethodList *next;
};

struct Binding {
  size_t refs;
  int weak;
  Value value;
};

static ModuleMap loaded_modules = NULL_HASH_MAP;

Module *keyword_module = NULL;

static void init_modules(void) {
  init_module_map(&loaded_modules);
  init_lang_module();
  keyword_module = create_module("keyword");
}

void unload_modules(void) {
  ModuleMapEntry entry;
  HashMapIterator it = module_map_iterate(&loaded_modules);
  while (module_map_next_entry(&it, &entry)) {
    delete_module(entry.value);
  }
  delete_module_map(&loaded_modules);
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
  if (weak) {
    delete_value(value);
  }
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
    int found = 0;
    Value value;
    switch (scope->type) {
      case VALUE_SCOPE:
        found = namespace_get(&symbol->module->defs, symbol, &value);
        break;
      case TYPE_SCOPE:
        found = namespace_get(&symbol->module->type_defs, symbol, &value);
        break;
    }
    if (found) {
      delete_value(SYMBOL(symbol));
      return copy_value(value);
    }
  }
  raise_error(name_error, "undefined name: %s", TO_C_STRING(symbol->name));
  delete_value(SYMBOL(symbol));
  return undefined;
}

Value scope_get_macro(Scope *scope, Symbol *symbol) {
  if (symbol->module) {
    Value value;
    if (namespace_get(&symbol->module->macro_defs, symbol, &value)) {
      delete_value(SYMBOL(symbol));
      return copy_value(value);
    }
  }
  raise_error(name_error, "undefined macro: %s", TO_C_STRING(symbol->name));
  delete_value(SYMBOL(symbol));
  return undefined;
}

Value get_read_macro(Symbol *symbol) {
  if (symbol->module) {
    Value value;
    if (namespace_get(&symbol->module->read_macro_defs, symbol, &value)) {
      delete_value(SYMBOL(symbol));
      return copy_value(value);
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
  if (module_map_get(&loaded_modules, name, NULL)) {
    delete_value(STRING(name));
    raise_error(name_error, "module already defined: %s", TO_C_STRING(name));
    return NULL;
  }
  Module *module = malloc(sizeof(Module));
  if (!module) {
    delete_value(STRING(name));
    return NULL;
  }
  module->name = name;
  init_symmap(&module->internal);
  init_symmap(&module->external);
  init_namespace(&module->defs);
  init_namespace(&module->macro_defs);
  init_namespace(&module->type_defs);
  init_namespace(&module->read_macro_defs);
  init_method_map(&module->methods);
  module_map_add(&loaded_modules, module->name, module);
  return module;
}

static void delete_method_key(MethodKey key) {
  delete_value(SYMBOL(key.symbol));
  delete_type(key.type);
}

static void delete_method_list(MethodList *methods) {
  delete_type_array(methods->parameters);
  delete_value(methods->definition);
  if (methods->next) {
    delete_method_list(methods->next);
  }
  free(methods);
}

static void delete_methods(MethodMap *methods) {
  MethodMapEntry entry;
  HashMapIterator it = method_map_iterate(methods);
  while (method_map_next_entry(&it, &entry)) {
    delete_method_key(entry.key);
    delete_method_list(entry.value);
    free(entry.value);
  }
  delete_method_map(methods);
}

static void delete_symbols(SymMap *symbols) {
  SymMapEntry entry;
  HashMapIterator it = symmap_iterate(symbols);
  while (symmap_next_entry(&it, &entry)) {
    if (entry.value) {
      delete_value(SYMBOL(entry.value));
    }
  }
  delete_symmap(symbols);
}

static void delete_defs(Namespace *namespace) {
  NamespaceEntry entry;
  HashMapIterator it = namespace_iterate(namespace);
  while (namespace_next_entry(&it, &entry)) {
    delete_value(SYMBOL(entry.key));
    delete_value(entry.value);
  }
  delete_namespace(namespace);
}

void delete_module(Module *module) {
  delete_defs(&module->defs);
  delete_defs(&module->macro_defs);
  delete_defs(&module->type_defs);
  delete_defs(&module->read_macro_defs);
  delete_symbols(&module->internal);
  delete_symbols(&module->external);
  delete_methods(&module->methods);
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

Value module_find_method(Module *module, const Symbol *symbol, const TypeArray *parameters) {
  Type *key_type = copy_type(parameters->elements[0]);
  while (key_type) {
    MethodKey query = (MethodKey){ .symbol = (Symbol *)symbol, .type = key_type };
    MethodList *methods;
    if (method_map_get(&module->methods, query, &methods)) {
      Value method = undefined;
      const TypeArray *best_types = NULL;
      while (methods) {
        if (type_array_equals(parameters, methods->parameters)) {
          method = methods->definition;
          break;
        } else if (are_subtypes_of(parameters, methods->parameters)) {
          if (!best_types || are_subtypes_of(methods->parameters, best_types)) {
            method = methods->definition;
            best_types = methods->parameters;
          }
        }
        methods = methods->next;
      }
      delete_type(key_type);
      return method;
    }
    Type *next;
    if (key_type->type == TYPE_INSTANCE) {
      next = get_poly_instance(copy_generic(key_type->instance.type));
    } else {
      next = key_type->super;
    }
    delete_type(key_type);
    key_type = next;
  }
  return undefined;
}

Module *find_module(const String *name) {
  if (!HASH_MAP_INITIALIZED(loaded_modules)) {
    init_modules();
  }
  Module *module;
  if (!module_map_get(&loaded_modules, name, &module)) {
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
      Symbol *value;
      if (symmap_get(&module->external, symbol_name, &value)) {
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
  Symbol *value;
  if (symmap_get(&module->external, s, &value)) {
    delete_value(STRING(s));
    return copy_object(value);
  }
  value = module_intern_symbol(module, s);
  if (!value) {
    return NULL;
  }
  symmap_add(&module->external, value->name, copy_object(value));
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

Symbol *module_find_internal(Module *module, const String *s) {
  Symbol *value;
  if (symmap_get(&module->internal, s, &value)) {
    return copy_object(value);
  }
  return NULL;
}

Symbol *module_intern_symbol(Module *module, String *s) {
  Symbol *value;
  if (symmap_get(&module->internal, s, &value)) {
    delete_value(STRING(s));
  } else {
    value = create_symbol(s, module);
    if (!value) {
      return NULL;
    }
    symmap_add(&module->internal, value->name, value);
  }
  return copy_object(value);
}

Value list_external_symbols(Module *module) {
  Vector *v = create_vector(get_hash_map_size(&module->external));
  if (!v) {
    return undefined;
  }
  Symbol *symbol;
  HashMapIterator it = symmap_iterate(&module->external);
  size_t i = 0;
  while (symmap_next(&it, &symbol)) {
    v->cells[i++] = copy_value(SYMBOL(symbol));
  }
  return VECTOR(v);
}

char **get_symbols(Module *module) {
  size_t entries = get_hash_map_size(&module->internal);
  char **symbols = malloc((entries + 1) * sizeof(char *));
  size_t i = 0;
  symbols[entries] = NULL;
  Symbol *symbol;
  HashMapIterator it = symmap_iterate(&module->external);
  while (symmap_next(&it, &symbol)) {
    symbols[i++] = string_printf(TO_C_STRING(symbol->name));
  }
  return symbols;
}

static int import_method(Module *dest, Symbol *symbol, TypeArray *parameters, Value definition) {
  MethodList *m = allocate(sizeof(MethodList));
  if (!m) {
    delete_value(SYMBOL(symbol));
    delete_type_array(parameters);
    delete_value(definition);
    return 0;
  }
  m->parameters = parameters;
  m->definition = definition;
  MethodKey search_key = (MethodKey){ .symbol = symbol, .type = parameters->elements[0] };
  MethodMapEntry existing;
  if (method_map_remove_entry(&dest->methods, search_key, &existing)) {
    delete_value(SYMBOL(symbol));
    m->next = existing.value;
    method_map_add(&dest->methods, existing.key, m);
  } else {
    copy_type(parameters->elements[0]);
    m->next = NULL;
    method_map_add(&dest->methods, search_key, m);
  }
  return 1;
}

int import_methods(Module *dest, Module *src) {
  MethodMapEntry entry;
  HashMapIterator it = method_map_iterate(&src->methods);
  while (method_map_next_entry(&it, &entry)) {
    if (!import_method(dest, entry.key.symbol, entry.value->parameters, entry.value->definition)) {
      return 0;
    }
  }
  return 1;
}

void import_module(Module *dest, Module *src) {
  Symbol *symbol;
  HashMapIterator it = symmap_iterate(&src->external);
  while (symmap_next(&it, &symbol)) {
    // TODO: detect conflict
    if (symmap_add(&dest->internal, symbol->name, symbol)) {
      copy_value(SYMBOL(symbol));
    }
  }
  import_methods(dest, src);
}

void module_define(Symbol *s, Value value) {
  Value existing;
  if (namespace_remove(&s->module->defs, s, &existing)) {
    delete_value(SYMBOL(s));
    delete_value(existing);
  }
  namespace_add(&s->module->defs, s, value);
}

void module_define_macro(Symbol *s, Value value) {
  Value existing;
  if (namespace_remove(&s->module->macro_defs, s, &existing)) {
    delete_value(SYMBOL(s));
    delete_value(existing);
  }
  namespace_add(&s->module->macro_defs, s, value);
}

void module_define_type(Symbol *s, Value value) {
  Value existing;
  if (namespace_remove(&s->module->type_defs, s, &existing)) {
    delete_value(SYMBOL(s));
    delete_value(existing);
  }
  namespace_add(&s->module->type_defs, s, value);
}

void module_define_read_macro(Symbol *s, Value value) {
  Value existing;
  if (namespace_remove(&s->module->read_macro_defs, s, &existing)) {
    delete_value(SYMBOL(s));
    delete_value(existing);
  }
  namespace_add(&s->module->read_macro_defs, s, value);
}

void module_define_method(Module *module, Symbol *symbol, TypeArray *parameters, Value value) {
  import_method(module, symbol, parameters, value);
}

void module_ext_define(Module *module, const char *name, Value value) {
  Symbol *symbol = module_extern_symbol(module, c_string_to_string(name));
  module_define(symbol, value);
}

void module_ext_define_macro(Module *module, const char *name, Value value) {
  Symbol *symbol = module_extern_symbol(module, c_string_to_string(name));
  module_define_macro(symbol, value);
}

void module_ext_define_type(Module *module, const char *name, Value value) {
  Symbol *symbol = module_extern_symbol(module, c_string_to_string(name));
  if (value.type == VALUE_TYPE && TO_TYPE(value)->name == NULL) {
    TO_TYPE(value)->name = copy_object(symbol);
  }
  module_define_type(symbol, value);
}

void module_ext_define_generic(Module *module, const char *name, uint8_t min_arity, uint8_t variadic, uint8_t type_parameters, int8_t *indices) {
  Symbol *symbol = module_extern_symbol(module, c_string_to_string(name));
  GenFunc *f = create_gen_func(copy_object(symbol), NULL, min_arity, variadic, type_parameters, indices);
  module_define(symbol, GEN_FUNC(f));
}

void module_ext_define_method(Module *module, const char *name, Value func, int type_parameters, ...) {
  va_list ap;
  Symbol *symbol = module_extern_symbol(module, c_string_to_string(name));
  TypeArray *types = create_type_array_null(type_parameters);
  va_start(ap, type_parameters);
  for (int i = 0; i < type_parameters; i++) {
    types->elements[i] = va_arg(ap, Type *);
  }
  va_end(ap);
  module_define_method(module, symbol, types, func);
}

static Hash method_key_hash(const MethodKey m) {
  Hash hash = INIT_HASH;
  hash = HASH_ADD_PTR(m.symbol, hash);
  hash = HASH_ADD_PTR(m.type, hash);
  return 0;
}

static int method_key_equals(const MethodKey a, const MethodKey b) {
  return a.symbol == b.symbol && a.type == b.type;
}

static Hash nse_string_hash(const String *s) {
  return string_hash(TO_C_STRING(s));
}

static int nse_string_equals(const String *a, const String *b) {
  return string_equals(TO_C_STRING(a), TO_C_STRING(b));
}

DEFINE_HASH_MAP(namespace, Namespace, Symbol *, Value, pointer_hash, pointer_equals)
DEFINE_HASH_MAP(symmap, SymMap, const String *, Symbol *, nse_string_hash, nse_string_equals)
DEFINE_HASH_MAP(module_map, ModuleMap, const String *, Module *, nse_string_hash, nse_string_equals)
DEFINE_HASH_MAP(method_map, MethodMap, MethodKey, MethodList *, method_key_hash, method_key_equals)
