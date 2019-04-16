/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "module.h"
#include "lang.h"
#include "error.h"
#include "eval.h"
#include "arg.h"
#include "type.h"

#include "special.h"

/* (if COND CONS ALT) */
Value eval_if(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length == 3) {
    Value condition = eval(copy_value(args.cells[0]), scope);
    if (RESULT_OK(condition)) {
      if (is_true(condition)) {
        result = eval(copy_value(args.cells[1]), scope);
      } else {
        result = eval(copy_value(args.cells[2]), scope);
      }
      delete_value(condition);
    }
  } else {
    raise_error(syntax_error, "expected (if ANY ANY ANY)");
  }
  delete_slice(args);
  return result;
}

/* (let ({(PATTERN EXPR)}) {EXPR}) */
Value eval_let(Slice args, Scope *scope) {
  Scope *let_scope = scope;
  Value result = undefined;
  if (args.length >= 1 && syntax_is(args.cells[0], VALUE_VECTOR)) {
    Vector *defs = TO_VECTOR(syntax_get(args.cells[0]));
    int ok = 1;
    // 1. Add all symbols to scope (for closures)
    for (size_t i = 0; i < defs->length; i++) {
      if (syntax_is(defs->cells[i], VALUE_VECTOR)) {
        Vector *def = TO_VECTOR(syntax_get(defs->cells[i]));
        if (def->length == 2) {
          if (syntax_is(def->cells[0], VALUE_SYMBOL)) {
            let_scope = scope_push(let_scope,
                TO_SYMBOL(copy_value(syntax_get(def->cells[0]))), undefined);
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
      // 2. Evaluate assignments
      for (size_t i = 0; i < defs->length; i++) {
        Vector *def = TO_VECTOR(syntax_get(defs->cells[i]));
        Value pattern = def->cells[0];
        Value assignment = eval(copy_value(def->cells[1]), let_scope);
        if (!RESULT_OK(assignment)) {
          ok = 0;
          break;
        }
        if (syntax_is(pattern, VALUE_SYMBOL)) {
          if (assignment.type == VALUE_CLOSURE) {
            // TODO: optimize
          }
          scope_set(let_scope, TO_SYMBOL(copy_value(syntax_get(pattern))),
              assignment, 0);
        } else if (!match_pattern(&let_scope, copy_value(pattern), assignment)) {
          ok = 0;
          break;
        }
      }
      if (ok) {
        // 3. Evaluate block
        result = eval_block(
            slice_slice(copy_slice(args), 1, args.length - 1),
            let_scope);
      }
    }
    scope_pop_until(let_scope, scope);
  } else {
    raise_error(syntax_error, "expected (let ({(PATTERN EXPR)}) {EXPR})");
  }
  delete_slice(args);
  return result;
}

/* (match EXPR {(PATTERN {EXPR})}) */
Value eval_match(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length >= 1) {
    Value value = eval(copy_value(args.cells[0]), scope);
    if (RESULT_OK(value)) {
      int no_match = 1;
      for (size_t i = 1; i < args.length; i++) {
        if (syntax_is(args.cells[i], VALUE_VECTOR)) {
          Vector *v = TO_VECTOR(syntax_get(args.cells[i]));
          if (v->length >= 1) {
            Scope *case_scope = scope;
            if (match_pattern(&case_scope, copy_value(v->cells[0]),
                  copy_value(value))) {
              result = eval_block(slice(VECTOR(copy_object(v)), 1, v->length - 1),
                  case_scope);
              scope_pop_until(case_scope, scope);
              no_match = 0;
              break;
            }
            scope_pop_until(case_scope, scope);
            continue;
          }
        }
        set_debug_form(copy_value(args.cells[i]));
        raise_error(syntax_error, "expected (PATTERN {EXPR})");
        no_match = 0;
        break;
      }
      if (no_match) {
        set_debug_form(copy_value(args.cells[0]));
        raise_error(syntax_error, "no match");
      }
      delete_value(value);
    }
  } else {
    raise_error(syntax_error, "expected (match EXPR {(PATTERN {EXPR})})");
  }
  delete_slice(args);
  return result;
}

static Value eval_anon(Slice args, Closure *closure) {
  Value definition = closure->env[0];
  Scope *scope = TO_POINTER(closure->env[1])->pointer;
  // TODO
  return undefined;
}

Value eval_fn(Slice args, Scope *scope) {
  Scope *fn_scope = copy_scope(scope);
  Value result = undefined;
  Value scope_ptr = check_alloc(POINTER(create_pointer(copy_type(scope_type), fn_scope, (Destructor) delete_scope)));
  if (RESULT_OK(scope_ptr)) {
    Value env[] = {slice_to_value(args), scope_ptr};
    // TODO
  }
  return undefined;
}

Value eval_try(Slice args, Scope *scope);

Value eval_continue(Slice args, Scope *scope);

Value eval_recur(Slice args, Scope *scope);

Value eval_def(Slice args, Scope *scope);

Value eval_def_read_macro(Slice args, Scope *scope);

Value eval_def_type(Slice args, Scope *scope);

Value eval_def_data(Slice args, Scope *scope);

Value eval_def_macro(Slice args, Scope *scope);

Value eval_def_generic(Slice args, Scope *scope);

Value eval_def_method(Slice args, Scope *scope);

Value eval_loop(Slice args, Scope *scope);

