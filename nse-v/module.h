/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_MODULE_H
#define NSE_MODULE_H

#include "value.h"

typedef struct Module Module;
typedef struct Type Type;
typedef struct TypeArray TypeArray;

typedef enum {
  TYPE_SCOPE,
  VALUE_SCOPE
} ScopeType;

typedef struct Scope Scope;

typedef struct Binding Binding;

struct Scope {
  Module *module;
  Symbol *symbol;
  Binding *binding;
  Scope *next;
  ScopeType type;
};

extern Module *lang_module;
extern Module *keyword_module;

void unload_modules();

Scope *scope_push(Scope *scope, Symbol *symbol, Value value);
Scope *scope_pop(Scope *scope);
void scope_pop_until(Scope *start, Scope *end);
Scope *copy_scope(Scope *scope);
void delete_scope(Scope *scope);
int scope_set(Scope *scope, Symbol *symbol, Value value, int weak);
Value scope_get(Scope *scope, Symbol *symbol);
Value scope_get_macro(Scope *scope, Symbol *symbol);
Value get_read_macro(Symbol *symbol);

Module *create_module(const char *name);
void delete_module(Module *module);
String *get_module_name(Module *module);
Scope *use_module(Module *module);
Scope *use_module_types(Module *module);
void module_define(Symbol *s, Value value);
void module_define_macro(Symbol *s, Value value);
void module_define_type(Symbol *s, Value value);
void module_define_read_macro(Symbol *s, Value value);
void module_define_method(Module *module, Symbol *symbol, TypeArray *parameters, Value value);
Symbol *module_ext_define(Module *module, const char *name, Value value);
Symbol *module_ext_define_macro(Module *module, const char *name, Value value);
Symbol *module_ext_define_type(Module *module, const char *name, Value value);

Value module_find_method(Module *module, Symbol *symbol, const TypeArray *parameters);

Module *find_module(const String *s);
Symbol *find_symbol(const String *s);
Symbol *module_find_internal(Module *module, const String *s);

Symbol *module_intern_symbol(Module *module, String *s);
Value list_external_symbols(Module *module);
char **get_symbols(Module *module);
Symbol *module_extern_symbol(Module *module, String *s);
Symbol *module_extern_symbol_c(Module *module, const char *s);
Symbol *intern_keyword(String *s);
void import_module(Module *dest, Module *src);
void import_module_symbol(Module *dest, Symbol *symbol);

#endif
