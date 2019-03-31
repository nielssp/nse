#ifndef NSE_SPECIAL_H
#define NSE_SPECIAL_H

#include "runtime/value.h"

NseVal eval_if(NseVal args, Scope *scope);
NseVal eval_let(NseVal args, Scope *scope);
NseVal eval_match(NseVal args, Scope *scope);
NseVal eval_fn(NseVal args, Scope *scope);
NseVal eval_try(NseVal args, Scope *scope);
NseVal eval_continue(NseVal args, Scope *scope);
NseVal eval_loop(NseVal args, Scope *scope);
NseVal eval_def(NseVal args, Scope *scope);
NseVal eval_def_read_macro(NseVal args, Scope *scope);
NseVal eval_def_type(NseVal args, Scope *scope);
NseVal eval_def_data(NseVal args, Scope *scope);
NseVal eval_def_macro(NseVal args, Scope *scope);

#endif
