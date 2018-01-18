#ifndef EVAL_H
#define EVAL_H

#include "util/hash_map.h"
#include "nsert.h"
#include "module.h"

NseVal eval(NseVal code, Scope *scope);

NseVal expand_macro_1(NseVal code, Scope *scope, int *expanded);
NseVal expand_macro(NseVal code, Scope *scope);

#endif
