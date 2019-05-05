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
#include "write.h"

#include "special.h"

Value eval_quote(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length == 1) {
    result = syntax_to_datum(copy_value(args.cells[0]));
  } else {
    raise_error(syntax_error, "expected (quote ANY)");
  }
  delete_slice(args);
  return result;
}

Value eval_type(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length == 1) {
    Scope *type_scope = use_module_types(scope->module);
    result = eval(copy_value(args.cells[0]), type_scope);
    scope_pop(type_scope);
  } else {
    raise_error(syntax_error, "expected (type ANY)");
  }
  delete_slice(args);
  return result;
}


static Value backquote_to_datum(Value v, Scope *scope);

static Value backquote_vector_to_datum(Vector *v, Scope *scope) {
  if (v->length == 0) {
    return VECTOR(v);
  }
  Value result = undefined;
  if (syntax_exact(v->cells[0], backquote_symbol)) {
    return syntax_to_datum(VECTOR(v));
  } else if (syntax_exact(v->cells[0], unquote_symbol)) {
    if (v->length == 2) {
      result = eval(copy_value(v->cells[1]), scope);
    } else {
      raise_error(syntax_error, "expected (unquote ANY)");
    }
    delete_value(VECTOR(v));
    return result;
  }
  Vector *splices = create_vector(v->length);
  size_t length = 0;
  if (splices) {
    int ok = 1;
    for (size_t i = 0; i < v->length; i++) {
      if (syntax_is(v->cells[i], VALUE_VECTOR)) {
        Vector *spliced = TO_VECTOR(syntax_get(v->cells[i]));
        if (spliced->length > 0 && syntax_exact(spliced->cells[0], splice_symbol)) {
          if (spliced->length == 2) {
            splices->cells[i] = eval(copy_value(spliced->cells[1]), scope);
            if (!RESULT_OK(splices->cells[i])) {
              ok = 0;
              break;
            } else if (splices->cells[i].type != VALUE_VECTOR) {
              set_debug_form(copy_value(spliced->cells[1]));
              raise_error(syntax_error, "expected VECTOR");
              ok = 0;
              break;
            }
            length += TO_VECTOR(splices->cells[i])->length;
            continue;
          } else {
            set_debug_form(copy_value(v->cells[i]));
            raise_error(syntax_error, "expected (splice VECTOR)");
            ok = 0;
            break;
          }
        }
      }
      length += 1;
    }
    if (ok) {
      Vector *final = create_vector(length);
      if (final) {
        size_t final_i = 0;
        for (size_t i = 0; i < v->length; i++) {
          if (RESULT_OK(splices->cells[i])) {
            Vector *splice = TO_VECTOR(splices->cells[i]);
            for (size_t j = 0; j < splice->length; j++) {
              final->cells[final_i++] = copy_value(splice->cells[j]);
            }
          } else {
            Value single = backquote_to_datum(copy_value(v->cells[i]), scope);
            if (!RESULT_OK(single)) {
              ok = 0;
              break;
            }
            final->cells[final_i++] = single;
          }
        }
        if (ok) {
          result = VECTOR(final);
        } else {
          delete_value(VECTOR(final));
        }
      }
    }
    delete_value(VECTOR(splices));
  }
  delete_value(VECTOR(v));
  return result;
}

static Value backquote_to_datum(Value v, Scope *scope) {
  Value result;
  switch (v.type) {
    case VALUE_SYNTAX: {
      Syntax *previous = push_debug_form(copy_value(v));
      result = backquote_to_datum(copy_value(TO_SYNTAX(v)->quoted), scope);
      delete_value(v);
      return pop_debug_form(result, previous);
    }
    case VALUE_VECTOR:
      return backquote_vector_to_datum(TO_VECTOR(v), scope);
    default:
      return v;
  }
}

Value eval_backquote(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length == 1) {
    result = backquote_to_datum(copy_value(args.cells[0]), scope);
  } else {
    raise_error(syntax_error, "expected (backquote ANY)");
  }
  delete_slice(args);
  return result;
}

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

static Value eval_anon(Slice args, const Closure *closure, Scope *dynamic_scope) {
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

Value eval_def_type(Slice args, Scope *scope) {
  raise_error(syntax_error, "not implemented");
  delete_slice(args);
  return undefined;
}

/* (def-macro (SYMBOL PARAMS) EXPR) */
Value eval_def_macro(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length >= 1 && syntax_is(args.cells[0], VALUE_VECTOR)) {
    Slice sig = to_slice(copy_value(syntax_get(args.cells[0])));
    if (sig.length >= 1 && syntax_is(sig.cells[0], VALUE_SYMBOL)) {
      Symbol *symbol = TO_SYMBOL(syntax_get(sig.cells[0]));
      Scope *fn_scope = copy_scope(scope);
      Value scope_ptr = check_alloc(POINTER(create_pointer(copy_type(scope_type),
              fn_scope, (Destructor) delete_scope)));
      if (RESULT_OK(scope_ptr)) {
        Vector *def = create_vector(args.length);
        if (def) {
          def->cells[0] = slice_to_value(
              slice_slice(copy_slice(sig), 1, sig.length - 1));
          for (size_t i = 1; i < args.length; i++) {
            def->cells[i] = copy_value(args.cells[i]);
          }
          Value env[] = {
            VECTOR(def),
            scope_ptr
          };
          result = check_alloc(CLOSURE(create_closure(eval_anon, env, 2)));
          delete_value(env[0]);
          delete_value(env[1]);
          if (RESULT_OK(result)) {
            module_define_macro(copy_object(symbol), copy_value(result));
          }
        } else {
          delete_value(scope_ptr);
        }
      } else {
        delete_scope(fn_scope);
      }
    } else {
      set_debug_form(copy_value(args.cells[0]));
      raise_error(syntax_error, "expected (SYMBOL ... PARAMS)");
    }
    delete_slice(sig);
  } else {
    raise_error(syntax_error, "expected (def-macro (SYMBOL ... PARAMS) EXPR)");
  }
  delete_slice(args);
  return result;
}

static Vector *get_constructor_parameter_types(Slice args, Scope *scope) {
  Vector *types = create_vector(args.length);
  int ok = 1;
  for (int i = 0; i < args.length; i++) {
    Type *t;
    if (syntax_is(args.cells[i], VALUE_SYMBOL)) {
      t = copy_type(any_type);
    } else if (syntax_is_special(args.cells[i], type_symbol, 1)) {
      Value type_value = eval(copy_value(syntax_get_elem(1, args.cells[i])), scope);
      if (type_value.type != VALUE_TYPE) {
        set_debug_form(copy_value(args.cells[i]));
        raise_error(syntax_error, "expected a type");
        ok = 0; break;
      }
      t = TO_TYPE(type_value);
    } else if (syntax_is(args.cells[i], VALUE_VECTOR)) {
      Vector *arg = TO_VECTOR(syntax_get(args.cells[i]));
      if (arg->length == 2 && syntax_is(arg->cells[0], VALUE_SYMBOL)
          && syntax_is_special(arg->cells[1], type_symbol, 1)) {
        Value type_value = eval(copy_value(syntax_get_elem(1, arg->cells[1])), scope);
        if (type_value.type != VALUE_TYPE) {
          set_debug_form(copy_value(arg->cells[1]));
          raise_error(syntax_error, "expected a type");
          ok = 0; break;
        }
        t = TO_TYPE(type_value);
      } else {
        set_debug_form(copy_value(args.cells[i]));
        raise_error(syntax_error, "expected (SYMBOL ^TYPE)");
        ok = 0; break;
      }
    } else {
      set_debug_form(copy_value(args.cells[i]));
      raise_error(syntax_error, "expected SYMBOL or ^TYPE or (SYMBOL ^TYPE)");
      ok = 0; break;
    }
    types->cells[i] = TYPE(t);
  }
  delete_slice(args);
  if (!ok) {
    delete_value(VECTOR(types));
    return NULL;
  }
  return types;
}

/* Checks if `actual` is a subtype of `formal` in the context of the generic
 * type `g`. If type variables are encountered inside `formal`, they are
 * assigned to the actual type via the `params` array.
 * Returns 1 if actual is a subtype of formal, 0 if not, -1 on error.
 * Parameters:
 *   actual     - The actual type.
 *   formal     - The formal type.
 *   g          - The generic type.
 *   invariant  - 1 if type check is invariant (i.e. actual must be equal to
 *                formal), 0 if type check is covariant (i.e. actual must be
 *                a subtype of formal).
 *   arity      - The arity of `g`.
 *   parameters - A pointer to an array of type parameters. The
 *                array must initialized to NULL by the caller. If no type
 *                parameters are encountered, the array will still be NULL after
 *                the function returns.
 */
static int is_instance_of(Type *actual, const Type *formal, const GType *g, int invariant, int arity, TypeArray **params) {
  int result;
  switch (formal->type) {
    case TYPE_SIMPLE:
    case TYPE_FUNC:
    case TYPE_POLY_INSTANCE:
      if (invariant) {
        result = actual == formal;
      } else {
        result = is_subtype_of(actual, formal);
      }
      break;
    case TYPE_INSTANCE: {
      if (actual->type == TYPE_POLY_INSTANCE && actual->poly_instance == formal->instance.type) {
        result = 1;
      } else if (actual->type != TYPE_INSTANCE
          || actual->instance.type != formal->instance.type) {
        if (invariant || !actual->super) {
          result = 0;
        } else {
          result = is_instance_of(copy_type(actual->super), formal, g, invariant, arity, params);
        }
      } else {
        result = 1;
        TypeArray *a_params = actual->instance.parameters;
        TypeArray *f_params = formal->instance.parameters;
        // Should always have the same arity
        for (int i = 0; i < a_params->size; i++) {
          result = is_instance_of(copy_type(a_params->elements[i]), f_params->elements[i], g, 1, arity, params);
          if (result != 1) {
            break;
          }
        }
      }
      break;
    }
    case TYPE_POLY_VAR: {
      if (formal->poly_var.type == g) {
        if (*params) {
          if ((*params)->elements[formal->poly_var.index]) {
            result = is_subtype_of(actual, (*params)->elements[formal->poly_var.index]);
          } else {
            (*params)->elements[formal->poly_var.index] = move_type(actual);
            return 1;
          }
        } else {
          *params = create_type_array_null(arity);
          if (!*params) {
            result = -1;
          } else {
            (*params)->elements[formal->poly_var.index] = move_type(actual);
            return 1;
          }
        }
      } else {
        result = actual == formal;
      }
    }
  }
  delete_type(actual);
  return result;
}

static void raise_parameter_type_error(const Symbol *function_name, Type *expected, Type *actual, int index, const GType *g, const TypeArray *params, Scope *scope) {
  char *function_name_s = nse_write_to_string(SYMBOL(function_name), scope->module);
  char *expected_s;
  if (params) {
    Type *expected_instance = instantiate_type(expected, g, params);
    expected_s = nse_write_to_string(TYPE(expected_instance), scope->module);
    delete_type(expected_instance);
  } else {
    expected_s = nse_write_to_string(TYPE(expected), scope->module);
    delete_type(expected);
  }
  char *actual_s = nse_write_to_string(TYPE(actual), scope->module);
  set_debug_arg_index(index);
  raise_error(domain_error, "%s expected parameter %d to be of type %s, not %s", function_name_s, index + 1, expected_s, actual_s);
  free(expected_s);
  free(actual_s);
  free(function_name_s);
  delete_type(actual);
}

static Value apply_constructor(Slice args, const Closure *closure, Scope *dynamic_scope) {
  Type *t = TO_TYPE(closure->env[0]);
  Symbol *tag = TO_SYMBOL(closure->env[1]);
  Vector *types = TO_VECTOR(closure->env[2]);
  Scope *scope = TO_POINTER(closure->env[3])->pointer;
  Value *record = NULL;
  int ok = 1;
  GType *g = NULL;
  TypeArray *g_params = NULL;
  int g_arity = 0;
  if (args.length != types->length) {
    char *tag_s = nse_write_to_string(SYMBOL(tag), scope->module);
    raise_error(domain_error, "%s expected %d parameters, but got %d", tag_s, types->length, args.length);
    free(tag_s);
    delete_slice(args);
    return undefined;
  }
  if (t->type == TYPE_POLY_INSTANCE) {
    g = t->poly_instance;
    g_arity = generic_type_arity(g);
  }
  if (types->length > 0) {
    record = allocate(sizeof(Value) * types->length);
    if (!record) {
      delete_slice(args);
      return undefined;
    }
    for (int i = 0; i < types->length; i++) {
      Type *formal = TO_TYPE(types->cells[i]);
      int check = is_instance_of(get_type(args.cells[i]), formal, g, 0, g_arity, &g_params);
      if (check < 0) {
        ok = 0;
        break;
      } else if (!check) {
        raise_parameter_type_error(tag, copy_type(formal), get_type(args.cells[i]), i, g, g_params, scope);
        ok = 0;
        break;
      }
      record[i] = args.cells[i];
    }
  }
  Value result = undefined;
  if (ok) {
    if (g_params) {
      t = get_instance(copy_generic(g), g_params);
      g_params = NULL;
      if (t) {
        Data *d = create_data(t, copy_object(tag), record, types->length);
        if (d) {
          result = DATA(d);
        }
      }
    } else {
      Data *d = create_data(copy_type(t), copy_object(tag), record, types->length);
      if (d) {
        result = DATA(d);
      }
    }
  }
  if (g_params) {
    delete_type_array(g_params);
  }
  if (record) {
    free(record);
  }
  delete_slice(args);
  return result;
}

static Value eval_def_data_constructor(Slice args, Type *t, Scope *scope) {
  Value result = undefined;
  if (syntax_is(args.cells[0], VALUE_SYMBOL)) {
    Symbol *tag = TO_SYMBOL(syntax_get(args.cells[0]));
    Vector *types = get_constructor_parameter_types(slice_slice(copy_slice(args), 1, args.length - 1), scope);
    if (types) {
      Scope *fn_scope = copy_scope(scope);
      Value scope_ptr = check_alloc(POINTER(create_pointer(copy_type(scope_type),
              fn_scope, (Destructor) delete_scope)));
      if (RESULT_OK(scope_ptr)) {
        Value env[] = {TYPE(t), copy_value(SYMBOL(tag)), VECTOR(types), scope_ptr};
        Value func = check_alloc(CLOSURE(create_closure(apply_constructor, env, 4)));
        if (RESULT_OK(func)) {
          module_define(copy_object(tag), copy_value(func));
          result = func;
        }
        delete_value(SYMBOL(tag));
        delete_value(scope_ptr);
      }
      delete_value(VECTOR(types));
    }
  } else {
    set_debug_form(copy_value(args.cells[0]));
    raise_error(syntax_error, "expected SYMBOL");
  }
  delete_slice(args);
  delete_type(t);
  return result;
}

static Value apply_generic_type(Slice args, const Closure *closure, Scope *dynamic_scope) {
  GType *g = TO_POINTER(closure->env[0])->pointer;
  if (args.length != generic_type_arity(g)) {
    raise_error(domain_error, "wrong number of parameters for generic type, expected %d, got %d", generic_type_arity(g), args.length);
    delete_slice(args);
    return undefined;
  }
  TypeArray *parameters = create_type_array_null(args.length);
  for (int i = 0; i < args.length; i++) {
    if (args.cells[i].type != VALUE_TYPE) {
      set_debug_arg_index(i);
      raise_error(domain_error, "generic type parameter must be a type");
      delete_type_array(parameters);
      delete_slice(args);
      return undefined;
    }
    parameters->elements[i] = copy_type(TO_TYPE(args.cells[i]));
  }
  delete_slice(args);
  return check_alloc(TYPE(get_instance(copy_generic(g), parameters)));
}

static GType *eval_def_generic_type(Vector *sig, Scope **scope) {
  if (sig->length > 1 && syntax_is(sig->cells[0], VALUE_SYMBOL)) {
    Symbol *name = TO_SYMBOL(syntax_get(sig->cells[0]));
    GType *g = create_generic(sig->length - 1, copy_type(any_type));
    if (g) {
      set_generic_type_name(g, copy_object(name));
      int ok = 1;
      for (int i = 1; i < sig->length; i++) {
        if (!syntax_is(sig->cells[i], VALUE_SYMBOL)) {
          set_debug_form(copy_value(sig->cells[i]));
          ok = 0; break;
        }
        Symbol *var_name = TO_SYMBOL(syntax_get(sig->cells[i]));
        Type *var = create_poly_var(copy_generic(g), i - 1);
        if (!var) {
          ok = 0; break;
        }
        *scope = scope_push(*scope, copy_object(var_name), TYPE(var));
      }
      if (ok) {
        Value g_ref = check_alloc(POINTER(create_pointer(copy_type(generic_type_type), copy_generic(g), (Destructor) delete_generic)));
        if (RESULT_OK(g_ref)) {
          Value env[] = {g_ref};
          Value func = check_alloc(CLOSURE(create_closure(apply_generic_type, env, 1)));
          if (RESULT_OK(func)) {
            module_define_type(copy_object(name), func);
            delete_value(g_ref);
            delete_value(VECTOR(sig));
            return g;
          }
          delete_value(g_ref);
        }
        delete_generic(g);
      }
    }
  } else {
    raise_error(syntax_error, "expected (SYMBOL ... PARAMS)");
  }
  delete_value(VECTOR(sig));
  return NULL;
}

static Value eval_def_data_generic(Vector *sig, Slice args, Scope *scope) {
  Value result = undefined;
  Scope *type_scope = use_module_types(scope->module);
  GType *g = eval_def_generic_type(sig, &type_scope);
  if (g) {
    Type *t = get_poly_instance(copy_generic(g));
    if (t) {
      int ok = 1;
      for (int i = 0; i < args.length; i++) {
        Value constructor = args.cells[i];
        if (syntax_is(constructor, VALUE_VECTOR) && TO_VECTOR(syntax_get(constructor))->length >= 1) {
          Value constructor_result = eval_def_data_constructor(to_slice(copy_value(syntax_get(constructor))),
              copy_type(t), type_scope);
          if (!RESULT_OK(constructor_result)) {
            ok = 0;
            break;
          }
          delete_value(constructor_result);
        } else if (syntax_is(constructor, VALUE_SYMBOL)) {
          Symbol *tag = TO_SYMBOL(syntax_get(constructor));
          Data *d = create_data(copy_type(t), copy_object(tag), NULL, 0);
          if (!d) {
            ok = 0;
            break;
          }
          module_define(tag, DATA(d));
        } else {
          ok = 0;
          set_debug_form(copy_value(constructor));
          raise_error(syntax_error, "expected SYMBOL or (SYMBOL ... PARAMS)");
          break;
        }
      }
      if (ok) {
        result = TYPE(t);
      } else {
        delete_type(t);
      }
    }
    delete_generic(g);
  }
  delete_scope(type_scope);
  delete_slice(args);
  return result;
}

static Value eval_def_data_nongeneric(Value head, Slice args, Scope *scope) {
  Value result = undefined;
  if (syntax_is(head, VALUE_SYMBOL)) {
    Symbol *s = TO_SYMBOL(syntax_get(head));
    Type *t = create_simple_type(copy_type(any_type));
    if (t) {
      t->name = copy_object(s);
      module_define_type(copy_object(s), copy_value(TYPE(t)));
      int ok = 1;
      for (int i = 0; i < args.length; i++) {
        Value constructor = args.cells[i];
        if (syntax_is(constructor, VALUE_VECTOR) && TO_VECTOR(syntax_get(constructor))->length >= 1) {
          Scope *type_scope = use_module_types(scope->module);
          Value constructor_result = eval_def_data_constructor(to_slice(copy_value(syntax_get(constructor))),
              copy_type(t), type_scope);
          delete_scope(type_scope);
          if (!RESULT_OK(constructor_result)) {
            ok = 0;
            break;
          }
          delete_value(constructor_result);
        } else if (syntax_is(constructor, VALUE_SYMBOL)) {
          Symbol *tag = TO_SYMBOL(syntax_get(constructor));
          Data *d = create_data(copy_type(t), copy_object(tag), NULL, 0);
          if (!d) {
            ok = 0;
            break;
          }
          module_define(tag, DATA(d));
        } else {
          ok = 0;
          set_debug_form(copy_value(constructor));
          raise_error(syntax_error, "expected SYMBOL or (SYMBOL ... PARAMS)");
          break;
        }
      }
      if (ok) {
        result = TYPE(t);
      } else {
        delete_type(t);
      }
    }
  } else {
    set_debug_form(copy_value(head));
    raise_error(syntax_error, "expected SYMBOL");
  }
  delete_value(head);
  delete_slice(args);
  return result;
}

/* (def-data SYMBOL {CONSTRUCTOR})
 * (def-data (SYMBOL {SYMBOL}) {CONSTRUCTOR}) */
Value eval_def_data(Slice args, Scope *scope) {
  Value result = undefined;
  if (args.length >= 1) {
    Value head = args.cells[0];
    if (syntax_is(head, VALUE_VECTOR)) {
      Syntax *previous = push_debug_form(copy_value(head));
      result = eval_def_data_generic(TO_VECTOR(copy_value(syntax_get(head))),
          slice_slice(copy_slice(args), 1, args.length - 1), scope);
      pop_debug_form(result, previous);
    } else {
      result = eval_def_data_nongeneric(copy_value(head),
          slice_slice(copy_slice(args), 1, args.length - 1), scope);
    }
  } else {
    raise_error(syntax_error, "expected (def-data SYMBOL ... CONSTRUCTORS)");
  }
  delete_slice(args);
  return result;
}


Value eval_def_generic(Slice args, Scope *scope) {
  return undefined;
}

Value eval_def_method(Slice args, Scope *scope) {
  return undefined;
}

Value eval_loop(Slice args, Scope *scope) {
  return undefined;
}

