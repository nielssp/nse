#ifndef EVAL_H
#define EVAL_H

#include "util/hash_map.h"
#include "nsert.h"

DECLARE_HASH_MAP(name_space, NameSpace, char *, NseVal *)

typedef struct scope Scope;
Scope *create_scope();
void delete_scope(Scope *scope);
NseVal scope_get(Scope *scope, const char *name);
void scope_define(Scope *scope, const char *name, NseVal value);

NseVal eval(NseVal code, Scope *scope);

#endif
