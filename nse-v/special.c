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
          // To fix circular references in closures we first assign the value to
          // the initial binding created in step 1. This weak binding is
          // available to all closures assigned in this let-expression.
          scope_set(let_scope, TO_SYMBOL(copy_value(syntax_get(pattern))),
              copy_value(assignment), 1 /* weak */);
          // We then push a new strong binding that's used when evaluating the
          // block in step 3.
          let_scope = scope_push(let_scope,
              TO_SYMBOL(copy_value(syntax_get(pattern))), assignment);
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

static Value eval_anon(Slice args, const Closure *closure) {
  Value result = undefined;
  Slice definition = to_slice(copy_value(closure->env[0]));
  if (definition.length >= 1) {
    Scope *scope = TO_POINTER(closure->env[1])->pointer;
    Scope *current_scope = scope;
    Slice formal = to_slice(copy_value(syntax_get(definition.cells[0])));
    if (assign_parameters(&current_scope, formal, copy_slice(args))) {
      result = eval_block(
          slice_slice(copy_slice(definition), 1, definition.length - 1),
          current_scope);
    }
    scope_pop_until(current_scope, scope);
  } else {
    raise_error(domain_error, "invalid function definition");
  }
  delete_slice(definition);
  delete_slice(args);
  return result;
}

/* (fn (PARAMS) {EXPR}) */
Value eval_fn(Slice args, Scope *scope) {
  if (args.length < 1 || !syntax_is(args.cells[0], VALUE_VECTOR)) {
    delete_slice(args);
    raise_error(syntax_error, "expected (fn (PARAMS) {EXPR})");
    return undefined;
  }
  Scope *fn_scope = copy_scope(scope);
  Value result = undefined;
  Value scope_ptr = check_alloc(POINTER(create_pointer(copy_type(scope_type),
          fn_scope, (Destructor) delete_scope)));
  if (RESULT_OK(scope_ptr)) {
    Value env[] = {slice_to_value(args), scope_ptr};
    result = check_alloc(CLOSURE(create_closure(eval_anon, env, 2)));
    delete_value(env[0]);
    delete_value(env[1]);
  } else {
    delete_scope(fn_scope);
    delete_slice(args);
  }
  return result;
}

static Type *get_result_type() {
  TypeArray *params = create_type_array(2, (Type * const []){ copy_type(any_type), copy_type(any_type) });
  return get_instance(copy_generic(result_type), params);
}

/* (try EXPR) */
Value eval_try(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length == 1) {
    Value ok_result = eval(copy_value(args.cells[0]), scope);
    if (RESULT_OK(ok_result)) {
      result = check_alloc(DATA(create_data(get_result_type(),
              copy_object(ok_symbol), (Value[]){ ok_result }, 1)));
      delete_value(ok_result);
    } else {
      Vector *error_vector = create_vector(4);
      if (error_vector) {
        error_vector->cells[0] = copy_value(SYMBOL(current_error_type()));
        error_vector->cells[1] = check_alloc(STRING(c_string_to_string(current_error())));
        error_vector->cells[2] = copy_value(check_alloc(SYNTAX(error_form)));
        error_vector->cells[3] = copy_value(check_alloc(LIST(get_stack_trace())));
        result = check_alloc(DATA(create_data(get_result_type(),
                copy_object(error_symbol), (Value[]){ VECTOR(error_vector) }, 1)));
        delete_value(VECTOR(error_vector));
      }
    }
  } else {
    raise_error(syntax_error, "expected (try EXPR)");
  }
  delete_slice(args);
  return result;
}

Value eval_continue(Slice args, Scope *scope);

Value eval_recur(Slice args, Scope *scope);

/* (def (SYMBOL PARAMS) {EXPR}) */
static Value eval_def_func(Vector *sig, Slice args, Scope *scope) {
  Value result = undefined;
  if (sig->length >= 1 && syntax_is(sig->cells[0], VALUE_SYMBOL)) {
    Symbol *symbol = TO_SYMBOL(syntax_get(sig->cells[0]));
    Scope *fn_scope = copy_scope(scope);
    Value scope_ptr = check_alloc(POINTER(create_pointer(copy_type(scope_type),
            fn_scope, (Destructor) delete_scope)));
    if (RESULT_OK(scope_ptr)) {
      Vector *def = create_vector(1 + args.length);
      if (def) {
        def->cells[0] = slice_to_value(slice(copy_value(VECTOR(sig)), 1, sig->length - 1));
        for (size_t i = 0; i < args.length; i++) {
          def->cells[i + 1] = copy_value(args.cells[i]);
        }
        Value env[] = {
          VECTOR(def),
          scope_ptr
        };
        result = check_alloc(CLOSURE(create_closure(eval_anon, env, 2)));
        delete_value(env[0]);
        delete_value(env[1]);
        if (RESULT_OK(result)) {
          // TODO: optimize
          module_define(copy_object(symbol), copy_value(result));
        }
      } else {
        delete_value(scope_ptr);
      }
    } else {
      delete_scope(fn_scope);
    }
  } else {
    raise_error(syntax_error, "expected (SYMBOL ... PARAMS)");
  }
  delete_value(VECTOR(sig));
  delete_slice(args);
  return result;
}

/* (def SYMBOL EXPR) */
static Value eval_def_var(Value name, Slice args, Scope *scope) {
  Value result = undefined;
  if (syntax_is(name, VALUE_SYMBOL) && args.length == 1) {
    Symbol *symbol = TO_SYMBOL(syntax_get(name));
    result = eval(copy_value(args.cells[0]), scope);
    if (RESULT_OK(result)) {
      module_define(copy_object(symbol), copy_value(result));
    }
  } else {
    raise_error(syntax_error, "expected (def SYMBOL EXPR)");
  }
  delete_value(name);
  delete_slice(args);
  return result;
}

Value eval_def(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length >= 1) {
    Value head = args.cells[0];
    if (syntax_is(head, VALUE_VECTOR)) {
      Syntax *previous = push_debug_form(copy_value(head));
      result = eval_def_func(TO_VECTOR(copy_value(syntax_get(head))),
          slice_slice(copy_slice(args), 1, args.length - 1), scope);
      pop_debug_form(result, previous);
    } else {
      result = eval_def_var(copy_value(head),
          slice_slice(copy_slice(args), 1, args.length - 1), scope);
    }
  } else {
    raise_error(syntax_error, "expected (def SYMBOL EXPR)");
  }
  delete_slice(args);
  return result;
}

/* (def-read-macro SYMBOL EXPR) */
Value eval_def_read_macro(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length == 2 && syntax_is(args.cells[0], VALUE_SYMBOL)) {
    Symbol *symbol = TO_SYMBOL(syntax_get(args.cells[0]));
    result = eval(copy_value(args.cells[1]), scope);
    if (RESULT_OK(result)) {
      module_define_read_macro(copy_object(symbol), copy_value(result));
    }
  } else {
    raise_error(syntax_error, "expected (def-read-macro SYMBOL EXPR)");
  }
  delete_slice(args);
  return result;
}

Value eval_def_type(Slice args, Scope *scope);

Value eval_def_data(Slice args, Scope *scope);

Value eval_def_macro(Slice args, Scope *scope);

Value eval_def_generic(Slice args, Scope *scope);

Value eval_def_method(Slice args, Scope *scope);

Value eval_loop(Slice args, Scope *scope);

