#include <string.h>

#include "eval.h"
#include "write.h"
#include "runtime/error.h"
#include "runtime/validate.h"

#include "special.h"

NseVal eval_if(NseVal args, Scope *scope) {
  NseVal result = undefined;
  NseVal condition = head(args);
  NseVal consequent = THEN(condition, elem(1, args));
  NseVal alternative = THEN(consequent, elem(2, args));
  NseVal condition_result = THEN(alternative, eval(condition, scope));
  if (RESULT_OK(condition_result)) {
    if (is_true(condition_result)) {
      result = eval(consequent, scope);
    } else {
      result = eval(alternative, scope);
    }
    del_ref(condition_result);
  }
  return result;
}

NseVal eval_let(NseVal args, Scope *scope) {
  Scope *let_scope = scope;
  NseVal result = undefined;
  NseVal defs = head(args);
  NseVal body = THEN(defs, tail(args));
  if (RESULT_OK(body)) {
    int ok = 1;
    while (is_cons(defs)) {
      NseVal pattern = head(head(defs));
      if (!RESULT_OK(pattern)) {
        ok = 0;
        break;
      }
      Symbol *symbol = to_symbol(pattern);
      if (symbol) {
        let_scope = scope_push(let_scope, symbol, undefined);
      }
      defs = tail(defs);
    }
    defs = head(args);
    while (is_cons(defs)) {
      NseVal pattern = head(head(defs));
      if (!RESULT_OK(pattern)) {
        ok = 0;
        break;
      }
      NseVal assignment = eval(elem(1, head(defs)), let_scope);
      if (!RESULT_OK(assignment)) {
        ok = 0;
        break;
      }
      if (assignment.type->internal == INTERNAL_CLOSURE && is_symbol(pattern)) {
        assignment.closure = optimize_tail_call(assignment.closure, to_symbol(pattern));
        if (!assignment.closure) {
          del_ref(assignment);
          ok = 0;
          break;
        }
      }
      Symbol *symbol = to_symbol(pattern);
      if (symbol) {
        scope_set(let_scope, symbol, assignment, 1);
      }
      if (!match_pattern(&let_scope, pattern, assignment)) {
        ok = 0;
      }
      del_ref(assignment);
      defs = tail(defs);
    }
    if (ok) {
      result = eval_block(body, let_scope);
    }
    scope_pop_until(let_scope, scope);
  }
  return result;
}

NseVal eval_match(NseVal args, Scope *scope) {
  NseVal result = undefined;
  NseVal h = head(args);
  NseVal value = THEN(h, eval(h, scope));
  if (RESULT_OK(value)) {
    NseVal cases = tail(args);
    if (is_cons(cases)) {
      for (; is_cons(cases); cases = tail(cases)) {
        NseVal c = head(cases);
        if (!is_cons(c)) {
          raise_error(syntax_error, "match case must be a list");
          break;
        }
        Scope *case_scope = scope;
        if (match_pattern(&case_scope, head(c), value)) {
          result = eval_block(tail(c), case_scope);
          scope_pop_until(case_scope, scope);
          break;
        }
        scope_pop_until(case_scope, scope);
      }
    }
    del_ref(value);
  }
  return result;
}

NseVal eval_fn(NseVal args, Scope *scope) {
  Scope *fn_scope = copy_scope(scope);
  NseVal result = undefined;
  NseVal scope_ref = check_alloc(REFERENCE(create_reference(copy_type(scope_type), fn_scope, (Destructor) delete_scope)));
  if (RESULT_OK(scope_ref)) {
    NseVal env[] = {args, scope_ref};
    CType *func_type = parameters_to_type(head(args));
    if (func_type) {
      result = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
    }
    del_ref(scope_ref);
  } else {
    delete_scope(fn_scope);
  }
  return result;
}

NseVal eval_try(NseVal args, Scope *scope) {
  NseVal h = head(args);
  if (RESULT_OK(h)) {
    NseVal result = eval(h, scope);
    if (RESULT_OK(result)) {
      NseVal tag = check_alloc(SYMBOL(intern_special("ok")));
      NseVal tail = check_alloc(CONS(create_cons(result, nil)));
      del_ref(result);
      NseVal output = check_alloc(CONS(create_cons(tag, tail))); 
      del_ref(tag);
      del_ref(tail);
      return output;
    } else {
      NseVal tag = add_ref(SYMBOL(current_error_type()));
      NseVal msg = check_alloc(STRING(create_string(current_error(), strlen(current_error()))));
      NseVal form = check_alloc(SYNTAX(error_form));
      NseVal tail1 = check_alloc(CONS(create_cons(get_stack_trace(), nil)));
      NseVal tail2 = check_alloc(CONS(create_cons(form, tail1)));
      NseVal tail3 = check_alloc(CONS(create_cons(msg, tail2)));
      del_ref(msg);
      del_ref(tail1);
      del_ref(tail2);
      NseVal output = check_alloc(CONS(create_cons(tag, tail3))); 
      del_ref(tag);
      del_ref(tail3);
      return output;
    }
  }
  return undefined;
}

NseVal eval_continue(NseVal args, Scope *scope) {
  NseVal result = undefined;
  NseVal arg_list = eval_list(args, scope);
  if (RESULT_OK(arg_list)) {
    result = check_alloc(CONTINUE(create_continue(arg_list)));
    del_ref(arg_list);
  }
  return result;
}

NseVal eval_recur(NseVal args, Scope *scope) {
  NseVal pattern = head(args);
  NseVal body = THEN(pattern, elem(1, args));
  Scope *loop_scope = scope;
  if (RESULT_OK(body)) {
    NseVal result = undefined;
    while (1) {
      result = eval(body, loop_scope);
      if (!RESULT_OK(result) || result.type != continue_type) {
        break;
      }
      scope_pop_until(loop_scope, scope);
      loop_scope = scope;
      if (!assign_parameters(&loop_scope, pattern, result.quote->quoted)) {
        del_ref(result);
        result = undefined;
        break;
      }
      del_ref(result);
    }
    scope_pop_until(loop_scope, scope);
    return result;
  }
  return undefined;
}

static NseVal eval_def_func(NseVal first, NseVal args, Scope *scope) {
  NseVal result = undefined;
  Symbol *symbol = to_symbol(head(first));
  if (symbol) {
    Scope *fn_scope = copy_scope(scope);
    NseVal scope_ref = check_alloc(REFERENCE(create_reference(copy_type(scope_type), fn_scope, (Destructor) delete_scope)));
    if (RESULT_OK(scope_ref)) {
      NseVal body = tail(args);
      NseVal formal = tail(first);
      if (RESULT_OK(formal) && RESULT_OK(body)) {
        String *doc_string = NULL;
        if (is_cons(body) && is_string(head(body))) {
          doc_string = to_string(head(body));
          body = tail(body);
        }
        NseVal def = check_alloc(CONS(create_cons(formal, body)));
        if (RESULT_OK(def)) {
          NseVal env[] = {def, scope_ref};
          CType *func_type = parameters_to_type(head(def));
          if (func_type) {
            NseVal func = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
            if (RESULT_OK(func)) {
              func.closure = optimize_tail_call(func.closure, symbol);
              if (func.closure) {
                if (doc_string) {
                  add_ref(STRING(doc_string));
                  func.closure->doc = doc_string;
                }
                module_define(symbol, func);
                result = add_ref(SYMBOL(symbol));
              }
              del_ref(func);
            }
          }
          del_ref(def);
        }
      }
      del_ref(scope_ref);
    } else {
      delete_scope(fn_scope);
    }
  } else {
    raise_error(syntax_error, "name of function must be a symbol");
  }
  return result;
}

static NseVal eval_def_var(NseVal first, NseVal args, Scope *scope) {
  Symbol *symbol = to_symbol(first);
  if (symbol) {
    NseVal expr = head(tail(args));
    NseVal value = THEN(expr, eval(expr, scope));
    if (RESULT_OK(value)) {
      module_define(symbol, value);
      del_ref(value);
      return add_ref(SYMBOL(symbol));
    }
  } else {
    raise_error(syntax_error, "name of constant must be a symbol");
  }
  return undefined;
}

NseVal eval_def(NseVal args, Scope *scope) {
  NseVal h = head(args);
  if (RESULT_OK(h)) {
    if (is_cons(h)) {
      return eval_def_func(h, args, scope);
    } else {
      return eval_def_var(h, args, scope);
    }
  } else {
    return undefined;
  }
}

NseVal eval_def_read_macro(NseVal args, Scope *scope) {
  NseVal h = head(args);
  if (RESULT_OK(h)) {
    Symbol *symbol = to_symbol(h);
    if (symbol) {
      NseVal value = eval(head(tail(args)), scope);
      if (RESULT_OK(value)) {
        module_define_read_macro(symbol, value);
        del_ref(value);
        return add_ref(SYMBOL(symbol));
      }
    } else {
      raise_error(syntax_error, "name of read macro must be a symbol");
    }
  }
  return undefined;
}

NseVal eval_def_type(NseVal args, Scope *scope) {
  // TODO: to be continued...
  raise_error(syntax_error, "not implemented");
  return undefined;
}

static NseVal get_constructor_parameter_types(NseVal args, Scope *scope, int *arity) {
  if (is_cons(args)) {
    (*arity)++;
    NseVal rest = get_constructor_parameter_types(tail(args), scope, arity);
    if (!RESULT_OK(rest)) {
      return rest;
    }
    NseVal arg = head(args);
    CType *t;
    if (is_symbol(arg)) {
      t = copy_type(any_type);
    } else {
      NseVal type_value;
      if (is_cons(arg)) {
        Symbol *sym = expect_elem_symbol(&arg);
        if (!sym) return undefined;
        TypeQuote *tq = expect_elem_type_quote(&arg);
        if (!tq) return undefined;
        if (!expect_nil(&arg)) return undefined;
        type_value = eval(tq->quoted, scope);
      } else if (is_type_quote(arg)) {
        type_value = eval(strip_syntax(arg).quote->quoted, scope);
      } else {
        del_ref(rest);
        set_debug_form(arg);
        raise_error(syntax_error, "constructor parameter must be a type, a symbol or a symbol and a type");
        return undefined;
      }
      if (!RESULT_OK(type_value)) {
        del_ref(rest);
        return undefined;
      }
      t = to_type(type_value);
      if (!t) {
        del_ref(rest);
        del_ref(type_value);
        set_debug_form(arg);
        raise_error(syntax_error, "parameter is not a valid type");
        return undefined;
      }
    }
    Cons *c = create_cons(TYPE(t), rest);
    delete_type(t);
    del_ref(rest);
    if (!c) {
      return undefined;
    }
    return CONS(c);
  } else if (is_nil(args)) {
    return nil;
  } else {
    set_debug_form(args);
    raise_error(syntax_error, "constructor parameters must be a proper list");
    return undefined;
  }
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
static int is_instance_of(CType *actual, const CType *formal, const GType *g, int invariant, int arity, CTypeArray **params) {
  int result;
  switch (formal->type) {
    case C_TYPE_SIMPLE:
    case C_TYPE_FUNC:
    case C_TYPE_CLOSURE:
    case C_TYPE_GFUNC:
    case C_TYPE_POLY_INSTANCE:
      if (invariant) {
        result = actual == formal;
      } else {
        result = is_subtype_of(actual, formal);
      }
      break;
    case C_TYPE_INSTANCE: {
      if (actual->type == C_TYPE_POLY_INSTANCE && actual->poly_instance == formal->instance.type) {
        result = 1;
      } else if (actual->type != C_TYPE_INSTANCE
          || actual->instance.type != formal->instance.type) {
        if (invariant || !actual->super) {
          result = 0;
        } else {
          result = is_instance_of(copy_type(actual->super), formal, g, invariant, arity, params);
        }
      } else {
        result = 1;
        CTypeArray *a_params = actual->instance.parameters;
        CTypeArray *f_params = formal->instance.parameters;
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
    case C_TYPE_POLY_VAR: {
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

static void raise_parameter_type_error(Symbol *function_name, CType *expected, CType *actual, int index, const GType *g, const CTypeArray *params, Scope *scope) {
  char *function_name_s = nse_write_to_string(SYMBOL(function_name), scope->module);
  char *expected_s;
  if (params) {
    CType *expected_instance = instantiate_type(expected, g, params);
    expected_s = nse_write_to_string(TYPE(expected_instance), scope->module);
    delete_type(expected_instance);
  } else {
    expected_s = nse_write_to_string(TYPE(expected), scope->module);
    delete_type(expected);
  }
  char *actual_s = nse_write_to_string(TYPE(actual), scope->module);
  raise_error(domain_error, "%s expects parameter %d to be of type %s, not %s", function_name_s, index, expected_s, actual_s);
  free(expected_s);
  free(actual_s);
  free(function_name_s);
  delete_type(actual);
}

static NseVal apply_constructor(NseVal args, NseVal env[]) {
  CType *t = env[0].type_val;
  Symbol *tag = env[1].symbol;
  int arity = env[2].i64;
  NseVal types = env[3];
  Scope *scope = env[4].reference->pointer;
  NseVal *record = NULL;
  int ok = 1;
  GType *g = NULL;
  CTypeArray *g_params = NULL;
  int g_arity = 0;
  if (t->type == C_TYPE_POLY_INSTANCE) {
    g = t->poly_instance;
    g_arity = generic_type_arity(g);
  }
  if (arity > 0) {
    record = allocate(sizeof(NseVal) * arity);
    if (!record) {
      return undefined;
    }
    int i = 0;
    while (is_cons(types)) {
      NseVal arg;
      if (!accept_elem_any(&args, &arg)) {
        raise_error(domain_error, "%s expects %d parameters, but got %d", tag->name, arity, i);
        ok = 0;
        break;
      }
      CType *formal = types.cons->head.type_val;
      int check = is_instance_of(copy_type(arg.type), formal, g, 0, g_arity, &g_params);
      if (check < 0) {
        ok = 0;
        break;
      } else if (!check) {
        raise_parameter_type_error(tag, copy_type(formal), copy_type(arg.type), i + 1, g, g_params, scope);
        ok = 0;
        break;
      }
      record[i++] = arg;
      types = tail(types);
    }
  }
  NseVal result = undefined;
  if (ok) {
    if (is_nil(args)) {
      if (g_params) {
        t = get_instance(copy_generic(g), move_type_array(g_params));
        g_params = NULL;
        if (t) {
          Data *d = create_data(t, tag, record, arity);
          if (d) {
            result = DATA(d);
          }
        }
      } else {
        Data *d = create_data(copy_type(t), tag, record, arity);
        if (d) {
          result = DATA(d);
        }
      }
    } else {
      raise_error(domain_error, "%s expects only %d parameters", tag->name, arity);
    }
  }
  if (g_params) {
    delete_type_array(g_params);
  }
  free(record);
  return result;
}

static NseVal eval_def_data_constructor(NseVal args, CType *t, Scope *scope) {
  Symbol *tag = to_symbol(head(args));
  if (tag) {
    NseVal result = undefined;
    int arity = 0;
    NseVal types = get_constructor_parameter_types(tail(args), scope, &arity);
    if (!RESULT_OK(types)) {
      return types;
    }
    Scope *fn_scope = copy_scope(scope);
    NseVal scope_ref = check_alloc(REFERENCE(create_reference(copy_type(scope_type), fn_scope, (Destructor) delete_scope)));
    if (RESULT_OK(scope_ref)) {
      NseVal env[] = {TYPE(t), SYMBOL(tag), I64(arity), types, scope_ref};
      CType *func_type = get_closure_type(arity, 0);
      if (func_type) {
        NseVal func = check_alloc(CLOSURE(create_closure(apply_constructor, func_type, env, 5)));
        if (RESULT_OK(func)) {
          module_define(tag, func);
          result = nil;
          del_ref(func);
        }
      }
      del_ref(scope_ref);
    } else {
      delete_scope(fn_scope);
    }
    del_ref(types);
    return result;
  } else {
    set_debug_form(head(args));
    raise_error(syntax_error, "name of constructor must be a symbol");
    return undefined;
  }
}

static NseVal apply_generic_type(NseVal args, NseVal env[]) {
  size_t arg_s;
  NseVal *arg_a = list_to_array(args, &arg_s);
  if (!arg_a) {
    return undefined;
  }
  GType *g = env[0].reference->pointer;
  if (arg_s != generic_type_arity(g)) {
    free(arg_a);
    raise_error(domain_error, "wrong number of parameters for generic type, expected %d, got %d", generic_type_arity(g), arg_s);
    return undefined;
  }
  CTypeArray *parameters = create_type_array_null(arg_s);
  for (int i = 0; i < arg_s; i++) {
    CType *t = to_type(arg_a[i]);
    if (!t) {
      raise_error(domain_error, "generic type parameter must be a type");
      free(arg_a);
      delete_type_array(parameters);
      return undefined;
    }
    parameters->elements[i] = copy_type(t);
  }
  free(arg_a);
  CType *instance = get_instance(copy_generic(g), move_type_array(parameters));
  return check_alloc(TYPE(instance));
}

static GType *eval_def_generic_type(NseVal args, Scope **scope) {
  Symbol *name = expect_elem_symbol(&args);
  if (name) {
    int arity = 0;
    Symbol *var;
    NseVal next_var = args;
    while (accept_elem_symbol(&next_var, &var)) {
      arity++;
    }
    if (is_nil(next_var) && arity != 0) {
      GType *g = create_generic(arity, INTERNAL_DATA, copy_type(any_type));
      set_generic_type_name(g, name);
      if (g) {
        for (int i = 0; i < arity; i++) {
          CType *var = create_poly_var(copy_generic(g), i);
          if (!var) {
            delete_generic(g);
            return NULL;
          }
          Symbol *var_name = to_symbol(elem(i, args));
          *scope = scope_push(*scope, var_name, TYPE(var));
          delete_type(var);
        }
        NseVal g_ref = check_alloc(REFERENCE(create_reference(copy_type(generic_type_type), copy_generic(g), (Destructor) delete_generic)));
        if (RESULT_OK(g_ref)) {
          NseVal env[] = {g_ref};
          CType *func_type = get_closure_type(arity, 0);
          if (func_type) {
            NseVal func = check_alloc(CLOSURE(create_closure(apply_generic_type, func_type, env, 1)));
            if (RESULT_OK(func)) {
              module_define_type(name, func);
              del_ref(func);
              del_ref(g_ref);
              return g;
            }
          }
          del_ref(g_ref);
        }
        delete_generic(g);
        return NULL;
      }
    } else {
      set_debug_form(args);
      raise_error(syntax_error, "generic type parameters must be a list containing at least one symbol");
    }
  } else {
    raise_error(syntax_error, "name of type must be a symbol");
  }
  return NULL;
}

NseVal eval_def_data(NseVal args, Scope *scope) {
  NseVal h = head(args);
  if (RESULT_OK(h)) {
    if (is_cons(h)) {
      NseVal result = nil;
      Scope *type_scope = use_module_types(scope->module);
      GType *g = eval_def_generic_type(h, &type_scope);
      if (g) {
        CType *t = get_poly_instance(copy_generic(g));
        for (NseVal c = tail(args); is_cons(c); c = tail(c)) {
          NseVal constructor = head(c);
          if (is_cons(constructor)) {
            NseVal constructor_result = eval_def_data_constructor(constructor, t, type_scope);
            if (!RESULT_OK(constructor_result)) {
              result = undefined;
              break;
            }
          } else {
            Symbol *tag = to_symbol(constructor);
            if (tag) {
              Data *d = create_data(copy_type(t), tag, NULL, 0);
              if (!d) {
                result = undefined;
                break;
              }
              module_define(tag, DATA(d));
              del_ref(DATA(d));
            } else {
              raise_error(syntax_error, "name of constructor must be a symbol");
              result = undefined;
              break;
            }
          }
        }
        delete_type(t);
        delete_generic(g);
      }
      delete_scope(type_scope);
      return result;
    } else {
      Symbol *symbol = to_symbol(h);
      if (symbol) {
        CType *t = create_simple_type(INTERNAL_DATA, copy_type(any_type));
        t->name = add_ref(SYMBOL(symbol)).symbol;
        module_define_type(symbol, TYPE(t));
        for (NseVal c = tail(args); is_cons(c); c = tail(c)) {
          NseVal constructor = head(c);
          if (is_cons(constructor)) {
            Scope *type_scope = use_module_types(scope->module);
            NseVal constructor_result = eval_def_data_constructor(constructor, t, type_scope);
            if (!RESULT_OK(constructor_result)) {
              delete_scope(type_scope);
              delete_type(t);
              return undefined;
            }
            delete_scope(type_scope);
          } else {
            Symbol *tag = to_symbol(constructor);
            if (tag) {
              Data *d = create_data(copy_type(t), tag, NULL, 0);
              if (!d) {
                delete_type(t);
                return undefined;
              }
              module_define(tag, DATA(d));
              del_ref(DATA(d));
            } else {
              delete_type(t);
              raise_error(syntax_error, "name of constructor must be a symbol");
              return undefined;
            }
          }
        }
        delete_type(t);
        return add_ref(SYMBOL(symbol));
      } else {
        raise_error(syntax_error, "name of type must be a symbol");
      }
    }
  }
  return undefined;
}

NseVal eval_def_macro(NseVal args, Scope *scope) {
  NseVal h = head(args);
  if (RESULT_OK(h)) {
    if (is_cons(h)) {
      Symbol *symbol = to_symbol(head(h));
      if (symbol) {
        Scope *macro_scope = copy_scope(scope);
        NseVal scope_ref = check_alloc(REFERENCE(create_reference(copy_type(scope_type), macro_scope, (Destructor) delete_scope)));
        if (RESULT_OK(scope_ref)) {
          NseVal body = tail(args);
          NseVal formal = tail(h);
          if (RESULT_OK(formal) && RESULT_OK(body)) {
            NseVal def = check_alloc(CONS(create_cons(formal, body)));
            if (RESULT_OK(def)) {
              NseVal env[] = {def, scope_ref};
              CType *func_type = parameters_to_type(head(def));
              if (func_type) {
                NseVal value = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
                del_ref(def);
                if (RESULT_OK(value)) {
                  module_define_macro(symbol, value);
                  del_ref(scope_ref);
                  del_ref(value);
                  return add_ref(SYMBOL(symbol));
                }
              }
            }
          }
          del_ref(scope_ref);
        } else {
          delete_scope(macro_scope);
        }
      } else {
        raise_error(syntax_error, "name of macro must be a symbol");
      }
    } else {
      raise_error(syntax_error, "macro must be a function");
    }
  }
  return undefined;
}

static CType *parameters_to_gfunc_type(NseVal formal) {
  int min_arity = 0;
  int variadic = 0;
  Symbol *param;
  while (accept_elem_symbol(&formal, &param)) {
    if (param == rest_keyword) {
      variadic = 1;
      if (!expect_elem_symbol(&formal)) {
        return NULL;
      }
      break;
    }
    min_arity++;
  }
  if (!is_nil(formal)) {
    set_debug_form(formal);
    raise_error(syntax_error, "formal parameters must be a proper list of symbols");
    return NULL;
  }
  return get_generic_func_type(min_arity, variadic);
}

/* (def-generic (SYMBOL {SYMBOL} [&rest SYMBOL])) */
NseVal eval_def_generic(NseVal args, Scope *scope) {
  Cons *sig = expect_elem_cons(&args);
  if (!sig) {
    return undefined;
  }
  NseVal sig_current = CONS(sig);
  Symbol *symbol = expect_elem_symbol(&sig_current);
  if (!symbol) {
    return undefined;
  }
  CType *t = parameters_to_gfunc_type(sig_current);
  if (!t) {
    return undefined;
  }
  GFunc *gfunc = create_gfunc(symbol, t, NULL);
  if (!gfunc) {
    return undefined;
  }
  module_define(symbol, GFUNC(gfunc));
  del_ref(GFUNC(gfunc));
  return add_ref(SYMBOL(symbol));
}

static NseVal get_method_parameters(NseVal args, int index, CTypeArray *types, int variadic, Scope *scope) {
  if (index >= types->size) {
    if (is_cons(args)) {
      set_debug_form(args);
      raise_error(domain_error, "too many parameters for method");
      return undefined;
    } else  if (is_nil(args)) {
      return nil;
    } else {
      set_debug_form(args);
      raise_error(syntax_error, "method parameters must be a proper list");
      return undefined;
    }
  }
  if (is_cons(args)) {
    NseVal rest = get_method_parameters(tail(args), index + 1, types, variadic, scope);
    if (!RESULT_OK(rest)) {
      return rest;
    }
    NseVal arg = head(args);
    if (variadic && index == types->size - 1) {
      if (!expect_elem_exact_symbol(&arg, rest_keyword)) {
        del_ref(rest);
        return undefined;
      }
    }
    Symbol *sym = expect_elem_symbol(&arg);
    TypeQuote *tq = THENP(sym, expect_elem_type_quote(&arg));
    NseVal type_value = THEN(check_alloc(TQUOTE(tq)), eval(tq->quoted, scope));
    if (!RESULT_OK(type_value) || !expect_nil(&arg)) {
      del_ref(rest);
      return undefined;
    }
    CType *t = to_type(type_value);
    if (!t) {
      del_ref(rest);
      del_ref(type_value);
      set_debug_form(arg);
      raise_error(syntax_error, "parameter is not a valid type");
      return undefined;
    }
    types->elements[index] = t;
    Cons *c = create_cons(SYMBOL(sym), rest);
    del_ref(rest);
    if (!c) {
      return undefined;
    }
    if (variadic && index == types->size - 1) {
      Cons *new_c = create_cons(SYMBOL(rest_keyword), CONS(c));
      del_ref(CONS(c));
      if (!new_c) {
        return undefined;
      }
      c = new_c;
    }
    return CONS(c);
  } else {
    set_debug_form(args);
    raise_error(syntax_error, "too few parameters for method");
    return undefined;
  }
}

/* (def-method (SYMBOL {(SYMBOL TQUOTE)} [&rest (SYMBOL TQUOTE)]) ANY) */
NseVal eval_def_method(NseVal args, Scope *scope) {
  Cons *sig = expect_elem_cons(&args);
  if (!sig) {
    return undefined;
  }
  NseVal sig_current = CONS(sig);
  Symbol *symbol = expect_elem_symbol(&sig_current);
  if (!symbol) {
    return undefined;
  }
  NseVal gfunc = scope_get(scope, symbol);
  if (!RESULT_OK(gfunc)) {
    return undefined;
  }
  if (gfunc.type->internal != INTERNAL_GFUNC) {
    set_debug_form(sig->head);
    char *function_name = nse_write_to_string(SYMBOL(symbol), scope->module);
    raise_error(domain_error, "%s is not a generic function", function_name);
    free(function_name);
    return undefined;
  }
  int variadic = gfunc.type->func.variadic;
  int arity = gfunc.type->func.min_arity + variadic;
  CTypeArray *types = create_type_array_null(arity);
  Scope *type_scope = use_module_types(scope->module);
  NseVal parameters = get_method_parameters(sig_current, 0, types, variadic, type_scope);
  delete_scope(type_scope);
  NseVal result = undefined;
  if (RESULT_OK(parameters)) {
    Scope *fn_scope = copy_scope(scope);
    NseVal scope_ref = check_alloc(REFERENCE(create_reference(copy_type(scope_type), fn_scope, (Destructor) delete_scope)));
    if (RESULT_OK(scope_ref)) {
      NseVal func_def = check_alloc(CONS(create_cons(parameters, args)));
      if (RESULT_OK(func_def)) {
        NseVal env[] = {func_def, scope_ref};
        CType *func_type = get_closure_type(arity - variadic, variadic);
        if (func_type) {
          NseVal func = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
          if (RESULT_OK(func)) {
            module_define_method(scope->module, symbol, copy_type_array(types), func);
            del_ref(func);
            result = add_ref(SYMBOL(symbol));
          }
        }
        del_ref(func_def);
      }
      del_ref(scope_ref);
    } else {
      delete_scope(fn_scope);
    }
    del_ref(parameters);
  }
  delete_type_array(types);
  return result;
}

static int eval_loop_ins(NseVal args, Scope *scope, ListBuilder *lb);

/* (for PATTERN EXPR) */
static int eval_loop_for(NseVal operands, NseVal rest, Scope *scope, ListBuilder *lb) {
  NseVal pattern, sequence;
  if (!accept_elem_any(&operands, &pattern)) {
    set_debug_form(operands);
    raise_error(syntax_error, "expected a pattern");
    return 0;
  }
  if (!accept_elem_any(&operands, &sequence)) {
    set_debug_form(operands);
    raise_error(syntax_error, "expected a sequence");
    return 0;
  }
  if (!expect_nil(&operands)) {
    return 0;
  }
  NseVal sequence_value = eval(sequence, scope);
  if (!RESULT_OK(sequence_value)) {
    return 0;
  }
  int ok = 1;
  NseVal current = sequence_value;
  while (ok && is_cons(current)) {
    Scope *loop_scope = scope;
    if (!match_pattern(&loop_scope, pattern, head(current))) {
      ok = 0;
    } else {
      current = tail(current);
      if (!eval_loop_ins(rest, loop_scope, lb)) {
        ok = 0;
      }
    }
    scope_pop_until(loop_scope, scope);
  }
  if (ok && !is_nil(current)) {
  }
  del_ref(sequence_value);
  return ok;
}

/* (let PATTERN EXPR) */
static int eval_loop_let(NseVal operands, NseVal rest, Scope *scope, ListBuilder *lb) {
  NseVal pattern, assignment;
  if (!accept_elem_any(&operands, &pattern)) {
    set_debug_form(operands);
    raise_error(syntax_error, "expected a pattern");
    return 0;
  }
  if (!accept_elem_any(&operands, &assignment)) {
    set_debug_form(operands);
    raise_error(syntax_error, "expected an assignment");
    return 0;
  }
  if (!expect_nil(&operands)) {
    return 0;
  }
  NseVal assignment_value = eval(assignment, scope);
  if (!RESULT_OK(assignment_value)) {
    return 0;
  }
  Scope *let_scope = scope;
  int result = 0;
  if (match_pattern(&let_scope, pattern, assignment_value)) {
    result = eval_loop_ins(rest, let_scope, lb);
  }
  scope_pop_until(let_scope, scope);
  del_ref(assignment_value);
  return result;
}

/* (if EXPR) */
static int eval_loop_if(NseVal operands, NseVal rest, Scope *scope, ListBuilder *lb) {
  NseVal condition;
  if (!accept_elem_any(&operands, &condition)) {
    set_debug_form(operands);
    raise_error(syntax_error, "expected a condition");
    return 0;
  }
  if (!expect_nil(&operands)) {
    return 0;
  }
  NseVal condition_value = eval(condition, scope);
  if (!RESULT_OK(condition_value)) {
    return 0;
  }
  int result = 1;
  if (is_true(condition_value)) {
    result = eval_loop_ins(rest, scope, lb);
  }
  del_ref(condition_value);
  return result;
}

/* (collect EXPR) */
static int eval_loop_collect(NseVal operands, NseVal rest, Scope *scope, ListBuilder *lb) {
  NseVal expr;
  if (!accept_elem_any(&operands, &expr)) {
    set_debug_form(operands);
    raise_error(syntax_error, "expected an expression");
    return 0;
  }
  if (!expect_nil(&operands)) {
    return 0;
  }
  NseVal expr_value = eval(expr, scope);
  if (!RESULT_OK(expr_value)) {
    return 0;
  }
  if (!list_builder_append(expr_value, lb)) {
    del_ref(expr_value);
    return 0;
  }
  del_ref(expr_value);
  return eval_loop_ins(rest, scope, lb);
}

/* (do {STMT}) */
static int eval_loop_do(NseVal operands, NseVal rest, Scope *scope, ListBuilder *lb) {
  if (RESULT_OK(eval_block(operands, scope))) {
    return eval_loop_ins(rest, scope, lb);
  }
  return 0;
}

static int eval_loop_ins(NseVal args, Scope *scope, ListBuilder *lb) {
  if (is_cons(args)) {
    Cons *ins = to_cons(head(args));
    if (!ins) {
      set_debug_form(head(args));
      raise_error(syntax_error, "loop instruction must be a list");
    } else {
      Symbol *op = to_symbol(ins->head);
      if (!op) {
        set_debug_form(ins->head);
        raise_error(syntax_error, "loop operator must be a symbol");
      } else if (op == for_symbol) {
        return eval_loop_for(ins->tail, tail(args), scope, lb);
      } else if (op == let_symbol) {
        return eval_loop_let(ins->tail, tail(args), scope, lb);
      } else if (op == if_symbol) {
        return eval_loop_if(ins->tail, tail(args), scope, lb);
      } else if (op == collect_symbol) {
        return eval_loop_collect(ins->tail, tail(args), scope, lb);
      } else if (op == do_symbol) {
        return eval_loop_do(ins->tail, tail(args), scope, lb);
      } else {
        set_debug_form(ins->head);
        raise_error(syntax_error, "unrecognized loop operator");
      }
    }
  } else if (is_nil(args)) {
    return 1;
  } else {
    set_debug_form(args);
    raise_error(syntax_error, "expected a proper list");
  }
  return 0;
}

/* (loop {LOOP_INS}) */
NseVal eval_loop(NseVal args, Scope *scope) {
  ListBuilder *lb = create_list_builder();
  if (!lb) {
    return undefined;
  }
  NseVal result = undefined;
  if (eval_loop_ins(args, scope, lb)) {
    result = list_builder_finalize(lb);
  }
  del_ref(LIST_BUILDER(lb));
  return result;
}
