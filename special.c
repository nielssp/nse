#include <string.h>

#include "eval.h"
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
  NseVal body = THEN(defs, elem(1, args));
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
      result = eval(body, let_scope);
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

NseVal eval_loop(NseVal args, Scope *scope) {
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
                module_define(symbol->module, symbol->name, func);
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
      module_define(symbol->module, symbol->name, value);
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
        module_define_read_macro(symbol->module, symbol->name, value);
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
        type_value = eval(TQUOTE(tq), scope);
      } else if (is_type_quote(arg)) {
        type_value = eval(arg, scope);
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
    del_ref(rest);
    if (!c) {
      delete_type(t);
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

static NseVal apply_constructor(NseVal args, NseVal env[]) {
  CType *t = env[0].type_val;
  Symbol *tag = env[1].symbol;
  int arity = env[2].i64;
  NseVal types = env[3];
  NseVal *record = NULL;
  if (arity > 0) {
    record = allocate(sizeof(NseVal) * arity);
    if (!record) {
      return undefined;
    }
    int i = 0;
    while (is_cons(types)) {
      NseVal arg;
      if (!accept_elem_any(&args, &arg)) {
        raise_error(domain_error, "too few parameters for constructor");
        free(record);
        return undefined;
      }
      if (!is_subtype_of(arg.type, types.cons->head.type_val)) {
        // TODO: better error: which parameter? which type?
        raise_error(domain_error, "parameter has incorrect type");
        free(record);
        return undefined;
      }
      record[i++] = arg;
      types = tail(types);
    }
  }
  NseVal result = undefined;
  if (is_nil(args)) {
    Data *d = create_data(copy_type(t), tag, record, arity);
    if (d) {
      result = DATA(d);
    }
  } else {
    raise_error(domain_error, "too many parameters for constructor");
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
        NseVal func = check_alloc(CLOSURE(create_closure(apply_constructor, func_type, env, 4)));
        if (RESULT_OK(func)) {
          module_define(tag->module, tag->name, func);
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

NseVal eval_def_data(NseVal args, Scope *scope) {
  NseVal h = head(args);
  if (RESULT_OK(h)) {
    if (is_cons(h)) {
      Symbol *symbol = to_symbol(head(h));
      if (symbol) {
        // TODO: not implemented
      } else {
        raise_error(syntax_error, "name of type must be a symbol");
      }
    } else {
      Symbol *symbol = to_symbol(h);
      if (symbol) {
        CType *t = create_simple_type(INTERNAL_DATA, copy_type(any_type));
        t->name = add_ref(SYMBOL(symbol)).symbol;
        module_define_type(symbol->module, symbol->name, TYPE(t));
        for (NseVal c = tail(args); is_cons(c); c = tail(c)) {
          NseVal constructor = head(c);
          if (is_cons(constructor)) {
            NseVal constructor_result = eval_def_data_constructor(constructor, t, scope);
            if (!RESULT_OK(constructor_result)) {
              delete_type(t);
              return undefined;
            }
          } else {
            Symbol *tag = to_symbol(constructor);
            if (tag) {
              Data *d = create_data(copy_type(t), tag, NULL, 0);
              if (!d) {
                delete_type(t);
                return undefined;
              }
              module_define(tag->module, tag->name, DATA(d));
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
              CType *func_type = get_closure_type(0, 1);
              NseVal value = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
              del_ref(scope_ref);
              del_ref(def);
              if (RESULT_OK(value)) {
                module_define_macro(symbol->module, symbol->name, value);
                del_ref(value);
                return add_ref(SYMBOL(symbol));
              }
            }
          }
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
