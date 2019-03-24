#include <string.h>

#include "runtime/value.h"
#include "runtime/error.h"
#include "write.h"

#include "eval.h"

NseVal eval_list(NseVal list, Scope *scope) {
  if (list.type->internal == INTERNAL_SYNTAX) {
    Syntax *previous = push_debug_form(list.syntax);
    return pop_debug_form(eval_list(list.syntax->quoted, scope), previous);
  } else if (list.type->internal == INTERNAL_NIL) {
    return nil;
  } else if (list.type->internal == INTERNAL_CONS) {
    NseVal result = undefined;
    NseVal head = eval(list.cons->head, scope);
    if (RESULT_OK(head)) {
      NseVal tail = eval_list(list.cons->tail, scope);
      if (RESULT_OK(tail)) {
        Cons *cons = create_cons(head, tail);
        if (cons != NULL) {
          result = CONS(cons);
        }
        del_ref(tail);
      }
      del_ref(head);
    }
    return result;
  }
  return eval(list, scope);
}

static CType *parameters_to_type(NseVal formal) {
  if (!formal.type) {
    return NULL;
  }
  switch (formal.type->internal) {
    case INTERNAL_NIL:
      return copy_type(nil_type);
    case INTERNAL_CONS:
      return copy_type(cons_type);
    case INTERNAL_SYMBOL:
      return copy_type(any_type);
    case INTERNAL_QUOTE: {
      NseVal datum = syntax_to_datum(formal.quote->quoted);
      CType *type = copy_type(datum.type);
      del_ref(datum);
      return type;
    }
    case INTERNAL_SYNTAX:
      return parameters_to_type(formal.syntax->quoted);
    default:
      raise_error(domain_error, "unexpected value type"); // TODO: type_to_string
      return NULL;
  }
}

struct named_parameter {
  Symbol *keyword;
  Symbol *symbol;
  NseVal default_value;
  int seen;
  struct named_parameter *next;
};

void delete_named_parameters(struct named_parameter *stack) {
  while (stack) {
    struct named_parameter *next = stack->next;
    free(stack);
    stack = next;
  }
}

int push_named_parameter(struct named_parameter **stack, Symbol *keyword, Symbol *symbol, NseVal default_value) {
  struct named_parameter *new = allocate(sizeof(struct named_parameter));
  if (!new) {
    delete_named_parameters(*stack);
    return 0;
  }
  new->keyword = keyword;
  new->symbol = symbol;
  new->next = *stack;
  new->default_value = default_value;
  new->seen = 0;
  *stack = new;
  return 1;
}

Symbol *find_named_parameter(struct named_parameter *stack, Symbol *keyword) {
  while (stack) {
    if (stack->keyword == keyword) {
      stack->seen = 1;
      return stack->symbol;
    }
    stack = stack->next;
  }
  return NULL;
}

int assign_named_parameters(Scope **scope, NseVal formal, NseVal actual) {
  int result = 1;
  struct named_parameter *params = NULL;
  while (is_cons(formal)) {
    Symbol *symbol;
    NseVal default_value = undefined;
    if (is_cons(head(formal))) {
      symbol = to_symbol(head(head(formal)));
      default_value = elem(1, head(formal));
      if (!RESULT_OK(default_value)) {
        raise_error(domain_error, "expected a default value");
        result = 0;
        break;
      }
    } else {
      symbol = to_symbol(head(formal));
    }
    if (!symbol) {
      raise_error(domain_error, "expected a symbol");
      result = 0;
      break;
    }
    Symbol *keyword = intern_keyword(symbol->name);
    if (!push_named_parameter(&params, keyword, symbol, default_value)) {
      result = 0;
      break;
    }
    formal = tail(formal);
  }
  if (result) {
    while (is_cons(actual)) {
      Symbol *keyword = to_keyword(head(actual));
      if (!keyword) {
        raise_error(domain_error, "expected a keyword");
        result = 0;
        break;
      }
      Symbol *symbol = find_named_parameter(params, keyword);
      if (!symbol) {
        raise_error(domain_error, "unknown named parameter: %s", keyword->name);
        result = 0;
        break;
      }
      NseVal value = elem(1, actual);
      if (!RESULT_OK(value)) {
        result = 0;
        break;
      }
      *scope = scope_push(*scope, symbol, value);
      actual = tail(tail(actual));
    }
    if (result) {
      for (struct named_parameter *stack = params; stack; stack = stack->next) {
        if (!stack->seen) {
          if (RESULT_OK(stack->default_value)) {
            NseVal default_value = eval(stack->default_value, *scope);
            if (!RESULT_OK(default_value)) {
              result = 0;
              break;
            }
            *scope = scope_push(*scope, stack->symbol, default_value);
          } else {
            *scope = scope_push(*scope, stack->symbol, nil);
          }
        }
      }
    }
  }
  delete_named_parameters(params);
  return result;
}

int assign_rest_parameters(Scope **scope, NseVal formal, NseVal actual) {
  Cons *cons = to_cons(formal);
  if (!cons || !is_nil(cons->tail)) {
    raise_error(domain_error, "&rest must be followed by exactly one symbol");
    return 0;
  }
  Symbol *name = to_symbol(cons->head);
  if (!name) {
    raise_error(domain_error, "&rest must be followed by exactly one symbol");
    return 0;
  }
  *scope = scope_push(*scope, name, actual);
  return 1;
}

int assign_opt_parameters(Scope **scope, NseVal formal, NseVal actual) {
  while (is_cons(formal)) {
    Symbol *symbol;
    NseVal default_expr = undefined;
    if (is_cons(head(formal))) {
      symbol = to_symbol(head(head(formal)));
      default_expr = elem(1, head(formal));
      if (!RESULT_OK(default_expr)) {
        raise_error(domain_error, "expected a default value");
        return 0;
      }
    } else {
      symbol = to_symbol(head(formal));
    }
    if (!symbol) {
      raise_error(domain_error, "expected a symbol");
      return 0;
    }
    if (symbol == key_symbol) {
      return assign_named_parameters(scope, tail(formal), actual);
    } else if (symbol == rest_symbol) {
      return assign_rest_parameters(scope, tail(formal), actual);
    }
    if (is_cons(actual)) {
      *scope = scope_push(*scope, symbol, head(actual));
      actual = tail(actual);
    } else if (RESULT_OK(default_expr)) {
      NseVal default_value = eval(default_expr, *scope);
      if (!RESULT_OK(default_value)) {
        return 0;
      }
      *scope = scope_push(*scope, symbol, default_value);
    } else {
      *scope = scope_push(*scope, symbol, nil);
    }
    formal = tail(formal);
  }
  if (!is_nil(actual)) {
    raise_error(pattern_error, "too many parameters for function");
    return 0;
  }
  return 1;
}

int assign_parameters(Scope **scope, NseVal formal, NseVal actual) {
  switch (formal.type->internal) {
    case INTERNAL_SYNTAX: {
      Syntax *previous = push_debug_form(formal.syntax);
      if (assign_parameters(scope, formal.syntax->quoted, actual)) {
        pop_debug_form(nil, previous);
        return 1;
      } else {
        pop_debug_form(undefined, previous);
        return 0;
      }
    }
    case INTERNAL_SYMBOL:
      *scope = scope_push(*scope, formal.symbol, actual);
      return 1;
    case INTERNAL_QUOTE:
      if (!is_true(nse_equals(formal.quote->quoted, actual))) {
        raise_error(pattern_error, "pattern match failed");
        return 0;
      }
      return 1;
    case INTERNAL_CONS: {
      Symbol *keyword = to_symbol(head(formal));
      if (keyword) {
        if (keyword == key_symbol) {
          return assign_named_parameters(scope, tail(formal), actual);
        } else if (keyword == opt_symbol) {
          return assign_opt_parameters(scope, tail(formal), actual);
        } else if (keyword == rest_symbol) {
          return assign_rest_parameters(scope, tail(formal), actual);
        }
      }
      if (!is_cons(actual)) {
        raise_error(pattern_error, "expected more parameters");
        return 0;
      }
      return assign_parameters(scope, head(formal), head(actual))
        && assign_parameters(scope, tail(formal), tail(actual));
    }
    case INTERNAL_NIL:
      if (!is_nil(actual)) {
        raise_error(pattern_error, "too many parameters for function");
        return 0;
      }
      return 1;
    default:
      // not ok
      return 0;
  }
}

NseVal eval_anon(NseVal args, NseVal env[]) {
  NseVal definition = env[0];
  Scope *scope = env[1].reference->pointer;
  NseVal formal = head(definition);
  NseVal body = head(tail(definition));
  Scope *current_scope = scope;
  NseVal result = undefined;
  if (assign_parameters(&current_scope, formal, args)) {
    result = eval(body, current_scope);
  }
  scope_pop_until(current_scope, scope);
  return result;
}

NseVal eval_anon_type(NseVal args, NseVal env[]) {
  NseVal name = env[2];
  Scope *scope = env[1].reference->pointer;
  NseVal name_cons = check_alloc(CONS(create_cons(name, args)));
  if (!RESULT_OK(name_cons)) {
    return undefined;
  }
  NseVal name_datum = syntax_to_datum(name_cons);
  del_ref(name_cons);
  if (!RESULT_OK(name_datum)) {
    return undefined;
  }
  NseVal result = eval_anon(args, env);
  if (RESULT_OK(result)) {
    if (is_type(result)) {
      CType *t = to_type(result);
      del_ref(result);
      result = TYPE(t);
    }
  }
  del_ref(name_datum);
  return result;
}

NseVal optimize_tail_call_any(NseVal code, Symbol *name);

NseVal optimize_tail_call_cons(Cons *cons, Symbol *name) {
  NseVal operator = cons->head;
  NseVal args = cons->tail;
  Symbol *macro_name = to_symbol(operator);
  if (macro_name) {
    if (macro_name == name) {
      //printf("optimizing tail call: %s\n", name->name);
      cons->head = add_ref(SYMBOL(continue_symbol));
      del_ref(operator);
      return TRUE;
    } else if (macro_name == if_symbol) {
      NseVal consequent = optimize_tail_call_any(elem(1, args), name);
      NseVal alternative = THEN(consequent, optimize_tail_call_any(elem(2, args), name));
      if (RESULT_OK(alternative)) {
        return is_true(consequent) ? TRUE : alternative;
      }
      return alternative;
    }
  }
  return FALSE;
}

NseVal optimize_tail_call_any(NseVal code, Symbol *name) {
  if (!code.type) {
    return code;
  }
  switch (code.type->internal) {
    case INTERNAL_CONS:
      return optimize_tail_call_cons(code.cons, name);
    case INTERNAL_I64:
    case INTERNAL_F64:
    case INTERNAL_STRING:
    case INTERNAL_QUOTE:
    case INTERNAL_SYMBOL:
      return FALSE;
    case INTERNAL_SYNTAX: {
      Syntax *previous = push_debug_form(code.syntax);
      return pop_debug_form(optimize_tail_call_any(code.syntax->quoted, name), previous);
    }
    default:
      raise_error(domain_error, "unexpected value type: %s", ""); // TOOD: type_to_str
      return undefined;
  }
}

Closure *optimize_tail_call(Closure *closure, Symbol *name) {
  if (closure->env_size != 2) { // FIXME
    return closure;
  }
  Cons *definition = to_cons(closure->env[0]);
  if (!definition) {
    return closure;
  }
  NseVal body = head(definition->tail);
  NseVal result = optimize_tail_call_any(body, name);
  if (RESULT_OK(result)) {
    if (is_true(result)) {
      NseVal loop1 = check_alloc(CONS(create_cons(body, nil)));
      NseVal loop2 = THEN(loop1, check_alloc(CONS(create_cons(definition->head, loop1))));
      del_ref(loop1);
      NseVal loop3 = THEN(loop2, check_alloc(CONS(create_cons(SYMBOL(loop_symbol), loop2))));
      del_ref(loop2);
      NseVal new_tail = THEN(loop3, check_alloc(CONS(create_cons(loop3, nil))));
      del_ref(loop3);
      if (RESULT_OK(new_tail)) {
        NseVal old_tail = definition->tail;
        definition->tail = new_tail;
        del_ref(old_tail);
      } else {
        del_ref(CLOSURE(closure));
        return NULL;
      }
    }
  }
  return closure;
}

NseVal eval_cons(Cons *cons, Scope *scope) {
  NseVal operator = cons->head;
  NseVal args = cons->tail;
  Symbol *macro_name = to_symbol(operator);
  if (macro_name) {
    if (macro_name == if_symbol) {
      NseVal result = undefined;
      NseVal condition = eval(head(args), scope);
      if (RESULT_OK(condition)) {
        if (is_true(condition)) {
          result = eval(head(tail(args)), scope);
        } else {
          result = eval(head(tail(tail(args))), scope);
        }
        del_ref(condition);
      }
      return result;
    } else if (macro_name == let_symbol) {
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
            // TODO: this is bad.. creates circular reference in closures,
            // e.g.(let ((f (fn (x) x))) f)
            scope_set(let_scope, symbol, assignment, 1);
          }
          if (!assign_parameters(&let_scope, pattern, assignment)) {
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
    } else if (macro_name == fn_symbol) {
      Scope *fn_scope = copy_scope(scope);
      NseVal result = undefined;
      NseVal scope_ref = check_alloc(REFERENCE(create_reference(copy_type(scope_type), fn_scope, (Destructor) delete_scope)));
      if (RESULT_OK(scope_ref)) {
        NseVal env[] = {args, scope_ref};
        CType *func_type = get_closure_type(0, 1);
        result = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
        del_ref(scope_ref);
      }
      return result;
    } else if (macro_name == try_symbol) {
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
    } else if (macro_name == continue_symbol) {
      NseVal result = undefined;
      NseVal arg_list = eval_list(args, scope);
      if (RESULT_OK(arg_list)) {
        result = check_alloc(CONTINUE(create_continue(arg_list)));
        del_ref(arg_list);
      }
      return result;
    } else if (macro_name == loop_symbol) {
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
    } else if (macro_name == def_symbol) {
      NseVal h = head(args);
      if (RESULT_OK(h)) {
        if (is_cons(h)) {
          Symbol *symbol = to_symbol(head(h));
          if (symbol) {
            Scope *fn_scope = copy_scope(scope);
            NseVal scope_ref = check_alloc(REFERENCE(create_reference(copy_type(scope_type), fn_scope, (Destructor) delete_scope)));
            if (RESULT_OK(scope_ref)) {
              NseVal body = tail(args);
              NseVal formal = tail(h);
              if (RESULT_OK(formal) && RESULT_OK(body)) {
                String *doc_string = NULL;
                if (is_cons(body) && is_string(head(body))) {
                  doc_string = to_string(head(body));
                  body = tail(body);
                }
                NseVal def = check_alloc(CONS(create_cons(formal, body)));
                if (RESULT_OK(def)) {
                  NseVal env[] = {def, scope_ref};
                  CType *func_type = get_closure_type(0, 1);
                  NseVal func = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
                  del_ref(scope_ref);
                  del_ref(def);
                  if (RESULT_OK(func)) {
                    func.closure = optimize_tail_call(func.closure, symbol);
                    if (func.closure) {
                      if (doc_string) {
                        add_ref(STRING(doc_string));
                        func.closure->doc = doc_string;
                      }
                      module_define(symbol->module, symbol->name, func);
                      del_ref(func);
                      return add_ref(SYMBOL(symbol));
                    }
                  }
                }
              }
            }
          } else {
            raise_error(syntax_error, "name of function must be a symbol");
          }
        } else {
          Symbol *symbol = to_symbol(h);
          if (symbol) {
            NseVal value = eval(head(tail(args)), scope);
            if (RESULT_OK(value)) {
              module_define(symbol->module, symbol->name, value);
              del_ref(value);
              return add_ref(SYMBOL(symbol));
            }
          } else {
            raise_error(syntax_error, "name of constant must be a symbol");
          }
        }
      }
      return undefined;
    } else if (macro_name == def_read_macro_symbol) {
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
    } else if (macro_name == def_type_symbol) {
      NseVal h = head(args);
      if (RESULT_OK(h)) {
        if (is_cons(h)) {
          Symbol *symbol = to_symbol(head(h));
          if (symbol) {
            Scope *fn_scope = copy_scope(scope);
            NseVal scope_ref = check_alloc(REFERENCE(create_reference(copy_type(scope_type), fn_scope, (Destructor) delete_scope)));
            if (RESULT_OK(scope_ref)) {
              NseVal body = tail(args);
              NseVal formal = tail(h);
              if (RESULT_OK(formal) && RESULT_OK(body)) {
                NseVal def = check_alloc(CONS(create_cons(formal, body)));
                if (RESULT_OK(def)) {
                  NseVal env[] = {def, scope_ref, head(h)};
                  CType *func_type = get_closure_type(0, 1);
                  NseVal func = check_alloc(CLOSURE(create_closure(eval_anon_type, func_type, env, 3)));
                  del_ref(scope_ref);
                  del_ref(def);
                  if (RESULT_OK(func)) {
                    module_define_type(symbol->module, symbol->name, func);
                    del_ref(func);
                    return add_ref(SYMBOL(symbol));
                  }
                }
              }
            }
          } else {
            raise_error(syntax_error, "name of type must be a symbol");
          }
        } else {
          Symbol *symbol = to_symbol(h);
          if (symbol) {
            NseVal value = eval(head(tail(args)), scope);
            if (RESULT_OK(value)) {
              if (is_type(value)) {
                CType *t = to_type(value);
                value = TYPE(t);
              }
              module_define_type(symbol->module, symbol->name, value);
              del_ref(value);
              return add_ref(SYMBOL(symbol));
            }
          } else {
            raise_error(syntax_error, "name of type must be a symbol");
          }
        }
      }
      return undefined;
    } else if (macro_name == def_macro_symbol) {
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
    NseVal macro_function = scope_get_macro(scope, macro_name);
    if (RESULT_OK(macro_function)) {
      NseVal result = undefined;
      NseVal expanded = nse_apply(macro_function, args);
      if (!RESULT_OK(expanded)) {
        return expanded;
      }
      result = eval(expanded, scope);
      del_ref(expanded);
      return result;
    }
  }
  NseVal result = undefined;
  NseVal function = eval(operator, scope);
  if (RESULT_OK(function)) {
    NseVal arg_list = eval_list(args, scope);
    if (RESULT_OK(arg_list)) {
      result = nse_apply(function, arg_list);
      del_ref(arg_list);
    }
    del_ref(function);
  }
  return result;
}

NseVal eval(NseVal code, Scope *scope) {
  switch (code.type->internal) {
    case INTERNAL_CONS:
      return eval_cons(code.cons, scope);
    case INTERNAL_I64:
    case INTERNAL_F64:
    case INTERNAL_STRING:
      return add_ref(code);
    case INTERNAL_QUOTE:
      if (code.type == type_quote_type) {
        Scope *type_scope = use_module_types(scope->module);
        NseVal result = eval(code.quote->quoted, type_scope);
        scope_pop(type_scope);
        return result;
      }
      if (scope->type == TYPE_SCOPE) {
        NseVal datum = syntax_to_datum(code.quote->quoted);
        if (!RESULT_OK(datum)) {
          return datum;
        }
        return check_alloc(TYPE(datum.type));
      }
      return syntax_to_datum(code.quote->quoted);
    case INTERNAL_SYMBOL:
      if (code.type == keyword_type) {
        return add_ref(code);
      }
      return add_ref(scope_get(scope, code.symbol));
    case INTERNAL_SYNTAX: {
      Syntax *previous = push_debug_form(code.syntax);
      return pop_debug_form(eval(code.syntax->quoted, scope), previous);
    }
    default:
      raise_error(domain_error, "unexpected value type: %s", ""); // TODO
      return undefined;
  }
}

NseVal expand_macro_1(NseVal code, Scope *scope, int *expanded) {
  if (!scope->module || !is_cons(code)) {
    return code;
  }
  NseVal macro = head(code);
  if (!is_symbol(macro)) {
    return code;
  }
  if (is_special_form(macro) ) {
    return code;
  }
  NseVal args = tail(code);
  NseVal function = scope_get_macro(scope, to_symbol(macro));
  if (!RESULT_OK(function)) {
    return code;
  }
  *expanded = 1;
  return nse_apply(function, args);
}

NseVal expand_macro(NseVal code, Scope *scope) {
  int expanded;
  do {
    expanded = 0;
    NseVal result = expand_macro_1(code, scope, &expanded);
    del_ref(code);
    code = result;
  } while (expanded && RESULT_OK(code));
  return code;
}
