#ifndef EVAL_H
#define EVAL_H

#include "runtime/value.h"
#include "module.h"

NseVal eval(NseVal code, Scope *scope);
NseVal eval_block(NseVal block, Scope *scope);
NseVal eval_anon(NseVal args, NseVal env[]);
NseVal eval_anon_type(NseVal args, NseVal env[]);
NseVal eval_list(NseVal list, Scope *scope);

CType *parameters_to_type(NseVal formal);
int assign_parameters(Scope **scope, NseVal formal, NseVal actual);

int match_pattern(Scope **scope, NseVal pattern, NseVal actual);

Closure *optimize_tail_call(Closure *closure, Symbol *name);
NseVal optimize_tail_call_any(NseVal code, Symbol *name);

NseVal expand_macro_1(NseVal code, Scope *scope, int *expanded);
NseVal expand_macro(NseVal code, Scope *scope);

#endif
