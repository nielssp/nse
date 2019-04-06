#include <string.h>

#include "runtime/value.h"
#include "runtime/error.h"
#include "runtime/validate.h"
#include "write.h"
#include "special.h"

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

CType *parameters_to_type(NseVal formal) {
  int ok = 1;
  int min_arity = 0;
  int optional = 0;
  int key = 0;
  int variadic = 0;
  Symbol *param;
  while (ok && accept_elem_symbol(&formal, &param)) {
    if (param == key_keyword) {
      key = 1;
      while (accept_elem_symbol(&formal, &param)) {
        // nop
      }
      break;
    } else if (param == opt_keyword) {
      while (ok && accept_elem_symbol(&formal, &param)) {
        if (param == key_keyword) {
          key = 1;
          while (accept_elem_symbol(&formal, &param)) {
            // nop
          }
          break;
        } else if (param == rest_keyword) {
          variadic = 1;
          if (!expect_elem_symbol(&formal)) {
            ok = 0;
          }
          break;
        }
        optional++;
      }
      break;
    } else if (param == rest_keyword) {
      variadic = 1;
      if (!expect_elem_symbol(&formal)) {
        ok = 0;
      }
      break;
    } else if (param == match_keyword) {
      if (!accept_elem_any(&formal, NULL)) {
        set_debug_form(formal);
        raise_error(syntax_error, "&match must be follwed by a pattern");
        ok = 0;
        break;
      }
    }
    min_arity++;
  }
  if (!ok) {
    return NULL;
  }
  if (!is_nil(formal)) {
    set_debug_form(formal);
    raise_error(syntax_error, "formal parameters must be a proper list of symbols");
    return NULL;
  }
  return get_closure_type(min_arity, variadic || key || optional);
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
    if (symbol == key_keyword) {
      return assign_named_parameters(scope, tail(formal), actual);
    } else if (symbol == rest_keyword) {
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

int match_pattern(Scope **scope, NseVal pattern, NseVal actual) {
  switch (pattern.type->internal) {
    case INTERNAL_SYNTAX: {
      Syntax *previous = push_debug_form(pattern.syntax);
      if (match_pattern(scope, pattern.syntax->quoted, actual)) {
        pop_debug_form(nil, previous);
        return 1;
      } else {
        pop_debug_form(undefined, previous);
        return 0;
      }
    }
    case INTERNAL_SYMBOL:
      *scope = scope_push(*scope, pattern.symbol, actual);
      return 1;
    case INTERNAL_QUOTE:
      if (actual.type->internal == INTERNAL_DATA) {
        Symbol *tag = to_symbol(pattern.quote->quoted);
        if (tag == actual.data->tag && actual.data->record_size == 0) {
          return 1;
        }
      }
      if (!is_true(nse_equals(pattern.quote->quoted, actual))) {
        raise_error(pattern_error, "pattern match failed");
        return 0;
      }
      return 1;
    case INTERNAL_CONS: {
      if (actual.type->internal == INTERNAL_DATA && is_quote(head(pattern))) {
        Symbol *tag = to_symbol(strip_syntax(head(pattern)).quote->quoted);
        if (tag == actual.data->tag) {
          NseVal next = tail(pattern);
          int match = 1;
          for (int i = 0; i < actual.data->record_size; i++) {
            if (!is_cons(next)) {
              match = 0;
              break;
            }
            if (!match_pattern(scope, head(next), actual.data->record[i])) {
              match = 0;
              break;
            }
            next = tail(next);
          }
          if (match) {
            if (!is_nil(next)) {
              set_debug_form(actual);
              raise_error(pattern_error, "pattern match failed");
              return 0;
            }
            return 1;
          }
        }
        set_debug_form(actual);
        raise_error(pattern_error, "pattern match failed");
        return 0;
      } else if (!is_cons(actual)) {
        set_debug_form(actual);
        raise_error(pattern_error, "expected list");
        return 0;
      }
      return match_pattern(scope, head(pattern), head(actual))
        && match_pattern(scope, tail(pattern), tail(actual));
    }
    case INTERNAL_NIL:
      if (!is_nil(actual)) {
        set_debug_form(actual);
        raise_error(pattern_error, "too many parameters for function");
        return 0;
      }
      return 1;
    case INTERNAL_I64:
    case INTERNAL_F64:
      if (!is_true(nse_equals(pattern, actual))) {
        raise_error(pattern_error, "pattern match failed");
        return 0;
      }
      return 1;
    default:
      // not ok
      return 0;
  }
}

int assign_parameters(Scope **scope, NseVal formal, NseVal actual) {
  while (is_cons(formal)) {
    NseVal h = head(formal);
    Symbol *param = to_symbol(head(formal));
    if (!param) {
      set_debug_form(h);
      raise_error(syntax_error, "expected a symbol");
      return 0;
    }
    if (param == key_keyword) {
      return assign_named_parameters(scope, tail(formal), actual);
    } else if (param == opt_keyword) {
      return assign_opt_parameters(scope, tail(formal), actual);
    } else if (param == rest_keyword) {
      return assign_rest_parameters(scope, tail(formal), actual);
    }
    if (!is_cons(actual)) {
      set_debug_form(actual);
      raise_error(domain_error, "too few parameters for function");
      return 0;
    }
    NseVal next = head(actual);
    if (param == match_keyword) {
      formal = tail(formal);
      Cons *cons = to_cons(formal);
      if (!cons) {
        set_debug_form(formal);
        raise_error(syntax_error, "&match must be followed by a pattern");
        return 0;
      }
      if (!match_pattern(scope, cons->head, next)) {
        return 0;
      }
    } else {
      *scope = scope_push(*scope, param, next);
    }
    formal = tail(formal);
    actual = tail(actual);
  }
  if (!is_nil(formal)) {
    set_debug_form(formal);
    raise_error(syntax_error, "formal parameters must be a proper list");
    return 0;
  }
  if (!is_nil(actual)) {
    set_debug_form(actual);
    raise_error(domain_error, "too many parameters for function");
    return 0;
  }
  return 1;
}

NseVal eval_block(NseVal block, Scope *scope) {
  NseVal result = nil;
  Scope *current_scope = scope;
  while (is_cons(block)) {
    del_ref(result);
    result = nil;
    NseVal statement = head(block);
    Symbol *name = NULL;
    NseVal expr = undefined;
    if (VALIDATE(statement, V_EXACT(let_symbol), V_SYMBOL(&name), V_ANY(&expr))) {
      NseVal value = eval(expr, current_scope);
      if (!RESULT_OK(value)) {
        result = value;
        break;
      }
      current_scope = scope_push(current_scope, name, value);
      del_ref(value);
    } else {
      result = eval(statement, current_scope);
      if (!RESULT_OK(result)) {
        break;
      }
    }
    block = tail(block);
  }
  scope_pop_until(current_scope, scope);
  return result;
}

NseVal eval_anon(NseVal args, NseVal env[]) {
  NseVal definition = env[0];
  Scope *scope = env[1].reference->pointer;
  NseVal formal = head(definition);
  Scope *current_scope = scope;
  NseVal result = undefined;
  if (assign_parameters(&current_scope, formal, args)) {
    result = eval_block(tail(definition), current_scope);
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
      return eval_if(args, scope);
    } else if (macro_name == let_symbol) {
      return eval_let(args, scope);
    } else if (macro_name == match_symbol) {
      return eval_match(args, scope);
    } else if (macro_name == do_symbol) {
      return eval_block(args, scope);
    } else if (macro_name == fn_symbol) {
      return eval_fn(args, scope);
    } else if (macro_name == try_symbol) {
      return eval_try(args, scope);
    } else if (macro_name == continue_symbol) {
      return eval_continue(args, scope);
    } else if (macro_name == loop_symbol) {
      return eval_loop(args, scope);
    } else if (macro_name == def_symbol) {
      return eval_def(args, scope);
    } else if (macro_name == def_read_macro_symbol) {
      return eval_def_read_macro(args, scope);
    } else if (macro_name == def_type_symbol) {
      return eval_def_type(args, scope);
    } else if (macro_name == def_data_symbol) {
      return eval_def_data(args, scope);
    } else if (macro_name == def_macro_symbol) {
      return eval_def_macro(args, scope);
    } else if (macro_name == def_generic_symbol) {
      return eval_def_generic(args, scope);
    } else if (macro_name == def_method_symbol) {
      return eval_def_method(args, scope);
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
      return syntax_to_datum(code.quote->quoted);
    case INTERNAL_SYMBOL:
      if (code.type == keyword_type) {
        return add_ref(code);
      }
      NseVal value = scope_get(scope, code.symbol);
      if (RESULT_OK(value) && value.type->internal == INTERNAL_GFUNC && !value.gfunc->context) {
        return check_alloc(GFUNC(create_gfunc(value.gfunc->name, copy_type(value.gfunc->type), scope->module)));
      }
      return add_ref(value);
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
