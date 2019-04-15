/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "module.h"
#include "lang.h"
#include "error.h"
#include "eval.h"

#include "special.h"

/* (if COND CONS ALT) */
Value eval_if(VectorSlice *args, Scope *scope) {
  Value result = undefined;
  if (args->length == 3) {
    Value condition = eval(copy_value(args->cells[0]), scope);
    if (RESULT_OK(condition)) {
      if (is_true(condition)) {
        result = eval(copy_value(args->cells[1]), scope);
      } else {
        result = eval(copy_value(args->cells[2]), scope);
      }
      delete_value(condition);
    }
  } else {
    raise_error(syntax_error, "expected (if ANY ANY ANY)");
  }
  delete_value(VECTOR_SLICE(args));
  return result;
}

/* (let ({(PATTERN EXPR)}) {EXPR}) */
Value eval_let(VectorSlice *args, Scope *scope) {
  Scope *let_scope = scope;
  Value result = undefined;
  if (args->length >= 1 && syntax_is(args->cells[0], VALUE_VECTOR)) {
    Vector *defs = TO_VECTOR(syntax_get(args->cells[0]));
    int ok = 1;
    for (size_t i = 0; i < defs->length; i++) {
      if (syntax_is(defs->cells[i], VALUE_VECTOR)) {
        Vector *def = TO_VECTOR(syntax_get(defs->cells[i]));
        if (def->length == 2) {
          if (syntax_is(def->cells[0], VALUE_SYMBOL)) {
            let_scope = scope_push(let_scope, TO_SYMBOL(copy_value(syntax_get(def->cells[0]))), undefined);
          }
          continue;
        }
      }
      set_debug_form(copy_value(defs->cells[i]));
      raise_error(syntax_error, "expected (PATTERN EXPR)");
      ok = 0;
      break;
    }
    if (ok) {
      for (size_t i = 0; i < defs->length; i++) {
        Vector *def = TO_VECTOR(syntax_get(defs->cells[i]));
        Value pattern = def->cells[0];
        Value assignment = eval(copy_value(def->cells[1]), let_scope);
        if (!RESULT_OK(assignment)) {
          ok = 0;
          break;
        }
        if (syntax_is(pattern, VALUE_SYMBOL)) {
          scope_set(let_scope, TO_SYMBOL(copy_value(syntax_get(pattern))), assignment, 0);
        } else {
          delete_value(assignment);
        }
      }
      if (ok) {
        result = eval_block(slice_vector_slice(copy_object(args), 1, args->length - 1), let_scope);
      }
    }
    scope_pop_until(let_scope, scope);
  } else {
    raise_error(syntax_error, "expected (let ({(PATTERN EXPR)}) {EXPR})");
  }
  delete_value(VECTOR_SLICE(args));
  return result;
}

Value eval_match(VectorSlice *args, Scope *scope);

Value eval_fn(VectorSlice *args, Scope *scope);

Value eval_try(VectorSlice *args, Scope *scope);

Value eval_continue(VectorSlice *args, Scope *scope);

Value eval_recur(VectorSlice *args, Scope *scope);

Value eval_def(VectorSlice *args, Scope *scope);

Value eval_def_read_macro(VectorSlice *args, Scope *scope);

Value eval_def_type(VectorSlice *args, Scope *scope);

Value eval_def_data(VectorSlice *args, Scope *scope);

Value eval_def_macro(VectorSlice *args, Scope *scope);

Value eval_def_generic(VectorSlice *args, Scope *scope);

Value eval_def_method(VectorSlice *args, Scope *scope);

Value eval_loop(VectorSlice *args, Scope *scope);

