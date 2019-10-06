/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_MODULE_H
#define NSE_MODULE_H

#include "value.h"

typedef struct Module Module;
typedef struct Type Type;
typedef struct TypeArray TypeArray;

typedef struct Scope Scope;

typedef struct Binding Binding;

struct Scope {
  Module *module;
  Symbol *namespace;
  Symbol *symbol;
  Binding *binding;
  Scope *next;
};

extern Module *lang_module;
extern Module *keyword_module;

void unload_modules(void);

Scope *use_namespace(Scope *next, Symbol *namespace);
Scope *scope_push(Scope *scope, Symbol *symbol, Value value);
Scope *scope_pop(Scope *scope);
void scope_pop_until(Scope *start, Scope *end);
Scope *copy_scope(Scope *scope);
void delete_scope(Scope *scope);
int scope_set(Scope *scope, Symbol *symbol, Value value, int weak);
Value scope_get_in_namespace(Scope *scope, Symbol *symbol, /*const*/ Symbol *namespace_name);
Value scope_get(Scope *scope, Symbol *symbol);
Value scope_get_macro(Scope *scope, Symbol *symbol);
Value get_read_macro(Symbol *symbol);

Module *create_module(const char *name);
void delete_module(Module *module);
String *get_module_name(Module *module);
Scope *use_module(Module *module);
Value module_define(Symbol *s, Value value);
Value namespace_define(Symbol *s, Value value, Symbol *namespace_name);
Value module_define_macro(Symbol *s, Value value);
Value module_define_type(Symbol *s, Value value);
Value module_define_read_macro(Symbol *s, Value value);
void module_define_method(Module *module, Symbol *symbol, TypeArray *parameters, Value value);

void module_ext_define(Module *module, const char *name, Value value);
void module_ext_define_macro(Module *module, const char *name, Value value);
void module_ext_define_type(Module *module, const char *name, Value value);

void module_ext_define_generic(Module *module, const char *name, uint8_t min_arity, uint8_t variadic, uint8_t type_parameters, int8_t *indices);
void module_ext_define_method(Module *module, const char *name, Value value, int type_parameters, ...);


Value module_find_method(Module *module, const Symbol *symbol, const TypeArray *parameters);

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
