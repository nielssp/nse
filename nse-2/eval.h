#ifndef EVAL_H
#define EVAL_H

#include "util/hash_map.h"
#include "nsert.h"

DECLARE_HASH_MAP(namespace, Namespace, char *, NseVal *)

typedef struct module Module;
typedef struct scope Scope;
Scope *scope_push(Scope *scope, const char *name, NseVal value);
Scope *scope_pop(Scope *scope);
NseVal scope_get(Scope *scope, const char *name);
Module *create_module(const char *name);
void delete_module(Module *module);
Scope *use_module(Module *module);
void module_define(Module *module, const char *name, NseVal value);
void module_define_type(Module *module, const char *name, NseVal value);

NseVal eval(NseVal code, Scope *scope);

NseVal expand_macro_1(NseVal code, Scope *scope, int *expanded);
NseVal expand_macro(NseVal code, Scope *scope);

#endif
