#include <string.h>

#include "nsert.h"
#include "write.h"

#include "eval.h"

NseVal eval_list(NseVal list, Scope *scope) {
  if (list.type == TYPE_SYNTAX) {
    Syntax *previous = push_debug_form(list.syntax);
    return pop_debug_form(eval_list(list.syntax->quoted, scope), previous);
  } else if (list.type == TYPE_NIL) {
    return nil;
  } else if (list.type == TYPE_CONS) {
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

static Type *parameters_to_type(NseVal formal) {
  switch (formal.type) {
    case TYPE_UNDEFINED:
      return NULL;
    case TYPE_NIL:
      return copy_type(nil_type);
    case TYPE_CONS:
      return create_cons_type(parameters_to_type(formal.cons->head), parameters_to_type(formal.cons->tail));
    case TYPE_SYMBOL:
      return copy_type(any_type);
    case TYPE_QUOTE: {
      NseVal datum = syntax_to_datum(formal.quote->quoted);
      Type *type = get_type(datum);
      del_ref(datum);
      return type;
    }
    case TYPE_SYNTAX:
      return parameters_to_type(formal.syntax->quoted);
    default:
      raise_error("unexpected value type: %s", nse_val_type_to_string(formal.type));
      return NULL;
  }
}

int assign_parameters(Scope **scope, NseVal formal, NseVal actual) {
  switch (formal.type) {
    case TYPE_SYNTAX:
      if (assign_parameters(scope, formal.syntax->quoted, actual)) {
        return 1;
      } else {
        return 0;
      }
    case TYPE_SYMBOL:
      *scope = scope_push(*scope, formal.symbol, actual);
      return 1;
    case TYPE_QUOTE:
      if (!is_true(nse_equals(formal.quote->quoted, actual))) {
        raise_error("pattern match failed");
        return 0;
      }
      return 1;
    case TYPE_CONS:
      if (!is_cons(actual)) {
        raise_error("too few parameters for function");
        return 0;
      }
      return assign_parameters(scope, head(formal), head(actual))
        && assign_parameters(scope, tail(formal), tail(actual));
    case TYPE_NIL:
      if (!is_nil(actual)) {
        raise_error("too many parameters for function");
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
      Type *t = to_type(result);
      char *type_name_str = nse_write_to_string(name_datum, scope->module);
      Type *alias = create_alias_type(type_name_str, copy_type(t));
      free(type_name_str);
      del_ref(result);
      result = TYPE(alias);
    }
  }
  del_ref(name_datum);
  return result;
}

NseVal eval_cons(Cons *cons, Scope *scope) {
  NseVal operator = cons->head;
  NseVal args = cons->tail;
  if (match_symbol(operator, SPECIAL_IF)) {
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
  } else if (match_symbol(operator, SPECIAL_LET)) {
  } else if (match_symbol(operator, SPECIAL_LAMBDA)) {
    Scope *fn_scope = copy_scope(scope);
    NseVal result = undefined;
    NseVal scope_ref = check_alloc(REFERENCE(create_reference(fn_scope, (Destructor) delete_scope)));
    if (RESULT_OK(scope_ref)) {
      NseVal env[] = {args, scope_ref};
      Type *arg_type = parameters_to_type(head(args));
      Type *func_type = create_func_type(arg_type, copy_type(any_type));
      result = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
      del_ref(scope_ref);
    }
    return result;
  } else if (match_symbol(operator, SPECIAL_TRY)) {
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
        NseVal tag = check_alloc(SYMBOL(intern_special("error")));
        NseVal msg = check_alloc(STRING(create_string(current_error(), strlen(current_error()))));
        NseVal form = check_alloc(SYNTAX(error_form));
        NseVal tail1 = check_alloc(CONS(create_cons(form, nil)));
        NseVal tail2 = check_alloc(CONS(create_cons(msg, tail1)));
        del_ref(msg);
        del_ref(tail1);
        NseVal output = check_alloc(CONS(create_cons(tag, tail2))); 
        del_ref(tag);
        del_ref(tail2);
        return output;
      }
    }
    return undefined;
  } else if (match_symbol(operator, SPECIAL_DEFINE)) {
    NseVal h = head(args);
    if (RESULT_OK(h)) {
      if (is_cons(h)) {
        Symbol *symbol = to_symbol(head(h));
        if (symbol) {
          Scope *fn_scope = copy_scope(scope);
          NseVal scope_ref = check_alloc(REFERENCE(create_reference(fn_scope, (Destructor) delete_scope)));
          if (RESULT_OK(scope_ref)) {
            NseVal body = tail(args);
            NseVal formal = tail(h);
            if (RESULT_OK(formal) && RESULT_OK(body)) {
              NseVal def = check_alloc(CONS(create_cons(formal, body)));
              if (RESULT_OK(def)) {
                NseVal env[] = {def, scope_ref};
                Type *arg_type = parameters_to_type(formal);
                Type *func_type = create_func_type(arg_type, copy_type(any_type));
                NseVal func = check_alloc(CLOSURE(create_closure(eval_anon, func_type, env, 2)));
                del_ref(scope_ref);
                del_ref(def);
                if (RESULT_OK(func)) {
                  module_define(symbol->module, symbol->name, func);
                  del_ref(func);
                  return add_ref(SYMBOL(symbol));
                }
              }
            }
          }
        } else {
          raise_error("name of function must be a symbol");
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
          raise_error("name of constant must be a symbol");
        }
      }
    }
    return undefined;
  } else if (match_symbol(operator, SPECIAL_DEFINE_TYPE)) {
    NseVal h = head(args);
    if (RESULT_OK(h)) {
      if (is_cons(h)) {
        Symbol *symbol = to_symbol(head(h));
        if (symbol) {
          Scope *fn_scope = copy_scope(scope);
          NseVal scope_ref = check_alloc(REFERENCE(create_reference(fn_scope, (Destructor) delete_scope)));
          if (RESULT_OK(scope_ref)) {
            NseVal body = tail(args);
            NseVal formal = tail(h);
            if (RESULT_OK(formal) && RESULT_OK(body)) {
              NseVal def = check_alloc(CONS(create_cons(formal, body)));
              if (RESULT_OK(def)) {
                NseVal env[] = {def, scope_ref, head(h)};
                Type *arg_type = parameters_to_type(formal);
                Type *func_type = create_func_type(arg_type, copy_type(any_type));
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
          raise_error("name of type must be a symbol");
        }
      } else {
        Symbol *symbol = to_symbol(h);
        if (symbol) {
          NseVal value = eval(head(tail(args)), scope);
          if (RESULT_OK(value)) {
            if (is_type(value)) {
              Type *t = to_type(value);
              Type *alias = create_alias_type(symbol->name, t);
              value = TYPE(alias);
            }
            module_define_type(symbol->module, symbol->name, value);
            del_ref(value);
            return add_ref(SYMBOL(symbol));
          }
        } else {
          raise_error("name of type must be a symbol");
        }
      }
    }
    return undefined;
  } else if (match_symbol(operator, SPECIAL_DEFINE_MACRO)) {
    NseVal h = head(args);
    if (RESULT_OK(h)) {
      if (is_cons(h)) {
        Symbol *symbol = to_symbol(head(h));
        if (symbol) {
          Scope *macro_scope = copy_scope(scope);
          NseVal scope_ref = check_alloc(REFERENCE(create_reference(macro_scope, (Destructor) delete_scope)));
          if (RESULT_OK(scope_ref)) {
            NseVal body = tail(args);
            NseVal formal = tail(h);
            if (RESULT_OK(formal) && RESULT_OK(body)) {
              NseVal def = check_alloc(CONS(create_cons(formal, body)));
              if (RESULT_OK(def)) {
                NseVal env[] = {def, scope_ref};
                Type *arg_type = parameters_to_type(formal);
                Type *func_type = create_func_type(arg_type, copy_type(any_type));
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
          raise_error("name of macro must be a symbol");
        }
      } else {
        raise_error("macro must be a function");
      }
    }
    return undefined;
  }
  NseVal result = undefined;
  if (is_symbol(operator)) {
    NseVal macro_function = scope_get_macro(scope, to_symbol(operator));
    if (RESULT_OK(macro_function)) {
      NseVal expanded = nse_apply(macro_function, args);
      if (!RESULT_OK(expanded)) {
        return expanded;
      }
      result = eval(expanded, scope);
      del_ref(expanded);
      return result;
    }
  }
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
  switch (code.type) {
    case TYPE_CONS:
      return eval_cons(code.cons, scope);
    case TYPE_I64:
    case TYPE_STRING:
    case TYPE_KEYWORD:
      return add_ref(code);
    case TYPE_QUOTE:
      if (scope->type == TYPE_SCOPE) {
        NseVal datum = syntax_to_datum(code.quote->quoted);
        if (!RESULT_OK(datum)) {
          return datum;
        }
        return check_alloc(TYPE(get_type(datum)));
      }
      return syntax_to_datum(code.quote->quoted);
    case TYPE_TQUOTE: {
      Scope *type_scope = use_module_types(scope->module);
      NseVal result = eval(code.quote->quoted, type_scope);
      scope_pop(type_scope);
      return result;
    }
    case TYPE_SYMBOL:
      return add_ref(scope_get(scope, code.symbol));
    case TYPE_SYNTAX: {
      Syntax *previous = push_debug_form(code.syntax);
      return pop_debug_form(eval(code.syntax->quoted, scope), previous);
    }
    default:
      raise_error("unexpected value type: %s", nse_val_type_to_string(code.type));
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
