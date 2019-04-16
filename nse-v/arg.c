/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "module.h"
#include "error.h"
#include "eval.h"
#include "lang.h"
#include "validate.h"
#include "type.h"

#include "arg.h"

typedef struct NamedParameter NamedParameter;

struct NamedParameter {
  Symbol *keyword;
  Symbol *symbol;
  Value default_value;
  int seen;
  NamedParameter *next;
};

static void delete_named_parameters(NamedParameter *stack) {
  while (stack) {
    NamedParameter *next = stack->next;
    delete_value(SYMBOL(stack->keyword));
    delete_value(SYMBOL(stack->symbol));
    delete_value(stack->default_value);
    free(stack);
    stack = next;
  }
}

static int push_named_parameter(NamedParameter **stack, Symbol *keyword, Symbol *symbol, Value default_value) {
  NamedParameter *new = allocate(sizeof(NamedParameter));
  if (!new) {
    delete_named_parameters(*stack);
    delete_value(SYMBOL(keyword));
    delete_value(SYMBOL(symbol));
    delete_value(default_value);
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

static Symbol *find_named_parameter(NamedParameter *stack, const Symbol *keyword) {
  while (stack) {
    if (stack->keyword == keyword) {
      stack->seen = 1;
      return stack->symbol;
    }
    stack = stack->next;
  }
  return NULL;
}

static int assign_named_parameters(Scope **scope, Slice formal, Slice actual) {
  int ok = 1;
  NamedParameter *params = NULL;
  for (size_t i = 0; i < formal.length; i++) {
    Symbol *symbol;
    Value default_value = undefined;
    if (syntax_is(formal.cells[i], VALUE_VECTOR)) {
      Vector *v = TO_VECTOR(syntax_get(formal.cells[i]));
      if (v->length == 2 && syntax_is(v->cells[0], VALUE_SYMBOL)) {
        symbol = TO_SYMBOL(copy_value(syntax_get(v->cells[0])));
        default_value = copy_value(v->cells[1]);
      } else {
        set_debug_form(copy_value(formal.cells[i]));
        raise_error(syntax_error, "expected (SYMBOL EXPR)");
        ok = 0;
        break;
      }
    } else if (syntax_is(formal.cells[i], VALUE_SYMBOL)) {
      symbol = TO_SYMBOL(copy_value(syntax_get(formal.cells[i])));
    } else {
      set_debug_form(copy_value(formal.cells[i]));
      raise_error(syntax_error, "expected a symbol"); 
      ok = 0;
      break;
    }
    Symbol *keyword = intern_keyword(copy_object(symbol->name));
    if (!push_named_parameter(&params, keyword, symbol, default_value)) {
      ok = 0;
      break;
    }
  }
  if (ok) {
    for (size_t i = 0; i < actual.length; i += 2) {
      if (!syntax_is(actual.cells[i], VALUE_KEYWORD)) {
        set_debug_arg_index(i);
        raise_error(domain_error, "expected a keyword");
        ok = 0;
        break;
      }
      if (i + 1 >= actual.length) {
        set_debug_arg_index(i);
        raise_error(domain_error, "keyword must be followed by a value");
        ok = 0;
        break;
      }
      Symbol *keyword = TO_SYMBOL(syntax_get(actual.cells[i]));
      Symbol *symbol = find_named_parameter(params, keyword);
      if (!symbol) {
        set_debug_arg_index(i);
        raise_error(domain_error, "unknown named parameter: %s", TO_C_STRING(keyword->name));
        ok = 0;
        break;
      }
      Value value = copy_value(actual.cells[i + 1]);
      *scope = scope_push(*scope, copy_object(symbol), value);
    }
    if (ok) {
      for (NamedParameter *stack = params; stack; stack = stack->next) {
        if (!stack->seen) {
          if (RESULT_OK(stack->default_value)) {
            Value default_value = eval(copy_value(stack->default_value), *scope);
            if (!RESULT_OK(default_value)) {
              ok = 0;
              break;;
            }
            *scope = scope_push(*scope, copy_object(stack->symbol), default_value);
          } else {
            *scope = scope_push(*scope, copy_object(stack->symbol), unit);
          }
        }
      }
    }
  }
  delete_named_parameters(params);
  delete_slice(formal);
  delete_slice(actual);
  return ok;
}

static int assign_rest_parameters(Scope **scope, Slice formal, Slice actual) {
  if (formal.length >= 1) {
    if (formal.length == 1 && syntax_is(formal.cells[0], VALUE_SYMBOL)) {
      Symbol *name = TO_SYMBOL(syntax_get(formal.cells[0]));
      *scope = scope_push(*scope, copy_object(name), slice_to_value(actual));
      delete_slice(formal);
      return 1;
    }
    set_debug_form(copy_value(formal.cells[0]));
  }
  raise_error(syntax_error, "&rest must be followed by exactly one symbol");
  delete_slice(formal);
  delete_slice(actual);
  return 0;
}

static int assign_opt_parameters(Scope **scope, Slice formal, Slice actual) {
  int ok = 1;
  size_t j = 0;
  for (size_t i = 0; i < formal.length; i++) {
    Symbol *symbol;
    Value default_expr = undefined;
    if (syntax_is(formal.cells[i], VALUE_VECTOR)) {
      Vector *v = TO_VECTOR(syntax_get(formal.cells[i]));
      if (v->length != 2 || !syntax_is(v->cells[0], VALUE_SYMBOL)) {
        set_debug_form(copy_value(formal.cells[i]));
        raise_error(syntax_error, "expected (SYMBOL EXPR)");
        ok = 0;
        break;
      }
      symbol = TO_SYMBOL(copy_value(syntax_get(v->cells[0])));
      default_expr = copy_value(v->cells[1]);
    } else if (syntax_is(formal.cells[i], VALUE_SYMBOL)) {
      symbol = TO_SYMBOL(copy_value(syntax_get(formal.cells[i])));
    } else {
      set_debug_form(copy_value(formal.cells[i]));
      raise_error(syntax_error, "expected a symbol");
      ok = 0;
      break;
    }
    if (symbol == key_keyword) {
      ok = assign_named_parameters(scope,
          slice_slice(copy_slice(formal), 1, formal.length - 1),
          slice_slice(copy_slice(actual), j, actual.length - j));
      if (error_arg_index >= 0) error_arg_index += j;
      j = actual.length;
      break;
    } else if (symbol == rest_keyword) {
      ok = assign_rest_parameters(scope,
          slice_slice(copy_slice(formal), 1, formal.length - 1),
          slice_slice(copy_slice(actual), j, actual.length - j));
      if (error_arg_index >= 0) error_arg_index += j;
      j = actual.length;
      break;
    }
    if (j < actual.length) {
      delete_value(default_expr);
      *scope = scope_push(*scope, symbol, copy_value(actual.cells[j]));
      j++;
    } else if (RESULT_OK(default_expr)) {
      Value default_value = eval(default_expr, *scope);
      if (!RESULT_OK(default_value)) {
        delete_value(SYMBOL(symbol));
        ok = 0;
        break;
      }
      *scope = scope_push(*scope, symbol, default_value);
    } else {
      *scope = scope_push(*scope, symbol, unit);
    }
  }
  if (ok && j < actual.length) {
    set_debug_arg_index(j);
    raise_error(domain_error, "too many parameters");
    ok = 0;
  }
  delete_slice(formal);
  delete_slice(actual);
  return ok;
}

int match_pattern(Scope **scope, Value pattern, Value actual) {
  int result = 1;
  switch (pattern.type) {
    case VALUE_SYNTAX: {
      Syntax *previous = push_debug_form(copy_value(pattern));
      if (match_pattern(scope, copy_value(TO_SYNTAX(pattern)->quoted), actual)) {
        pop_debug_form(unit, previous);
        delete_value(pattern);
        return 1;
      } else {
        pop_debug_form(undefined, previous);
        delete_value(pattern);
        return 0;
      }
    }
    case VALUE_SYMBOL:
      *scope = scope_push(*scope, TO_SYMBOL(pattern), actual);
      return 1;
    case VALUE_QUOTE:
      if (actual.type == VALUE_DATA && syntax_is(TO_QUOTE(pattern)->quoted, VALUE_SYMBOL)) {
        Symbol *tag = TO_SYMBOL(syntax_get(TO_QUOTE(pattern)->quoted));
        if (tag != TO_DATA(actual)->tag || TO_DATA(actual)->size != 0) {
          raise_error(pattern_error, "pattern match failed");
          result = 0;
        }
      } else if (equals(TO_QUOTE(pattern)->quoted, actual) != EQ_EQUAL) {
        raise_error(pattern_error, "pattern match failed");
        result = 0;
      }
      break;
    case VALUE_VECTOR: {
      Vector *p = TO_VECTOR(pattern);
      if (actual.type == VALUE_DATA) {
        Data *d = TO_DATA(actual);
        if (p->length == d->size + 1 && syntax_equals(p->cells[0], SYMBOL(d->tag))) {
          for (size_t i = 0; i < d->size; i++) {
            if (!match_pattern(scope, p->cells[i + 1], d->fields[i])) {
              result = 0;
              break;
            }
          }
        }
        raise_error(pattern_error, "pattern match failed");
      } else if (actual.type != VALUE_VECTOR) {
        raise_error(pattern_error, "expected vector");
        result = 0;
      } else {
        Vector *a = TO_VECTOR(actual);
        if (p->length != a->length) {
          raise_error(pattern_error, "expected vector of length %zd", p->length);
          result = 0;
        } else {
          for (size_t i = 0; i < p->length; i++) {
            if (!match_pattern(scope, copy_value(p->cells[i]), copy_value(a->cells[i]))) {
              result = 0;
              break;
            }
          }
        }
      }
      break;
    }
    case VALUE_UNIT:
    case VALUE_I64:
    case VALUE_F64:
    case VALUE_STRING:
    case VALUE_KEYWORD:
      if (equals(pattern, actual) != EQ_EQUAL) {
        raise_error(pattern_error, "pattern match failed");
        result = 0;
      }
      break;
    default:
      result = 0;
      break;
  }
  delete_value(pattern);
  delete_value(actual);
  return result;
}

int assign_parameters(Scope **scope, Slice formal, Slice actual) {
  int ok = 1;
  size_t j = 0;
  for (size_t i = 0; i < formal.length; i++) {
    Symbol *symbol;
    if (!validate(formal.cells[i], V_SYMBOL(&symbol))) {
      ok = 0;
      break;
    }
    if (symbol == key_keyword) {
      ok = assign_named_parameters(scope,
          slice_slice(copy_slice(formal), i, formal.length - i),
          slice_slice(copy_slice(actual), j, actual.length - j));
      if (error_arg_index >= 0) error_arg_index += j;
      j = actual.length;
    } else if (symbol == opt_keyword) {
      ok = assign_opt_parameters(scope,
          slice_slice(copy_slice(formal), i, formal.length - i),
          slice_slice(copy_slice(actual), j, actual.length - j));
      if (error_arg_index >= 0) error_arg_index += j;
      j = actual.length;
      break;
    } else if (symbol == rest_keyword) {
      ok = assign_rest_parameters(scope,
          slice_slice(copy_slice(formal), i, formal.length - i),
          slice_slice(copy_slice(actual), j, actual.length - j));
      if (error_arg_index >= 0) error_arg_index += j;
      j = actual.length;
    } else if (j >= actual.length) {
      raise_error(domain_error, "too few parameters");
      ok = 0;
      break;
    } else if (symbol == match_keyword) {
      if (i + 1 < formal.length) {
        ok = match_pattern(scope, copy_value(formal.cells[++i]),
            copy_value(actual.cells[j++]));
        if (!ok) break;
      } else {
        set_debug_form(copy_value(formal.cells[i]));
        raise_error(syntax_error, "&match must be followed by a pattern");
        ok = 0;
        break;
      }
    } else {
      *scope = scope_push(*scope, copy_object(symbol),
          copy_value(actual.cells[j++]));
    }
  }
  if (ok && j < actual.length) {
    set_debug_arg_index(j);
    raise_error(domain_error, "too many parameters");
    ok = 0;
  }
  delete_slice(formal);
  delete_slice(actual);
  return ok;
}

Type *parameters_to_type(Slice formal) {
  int ok = 1;
  int min_arity = 0;
  int optional = 0;
  int key = 0;
  int variadic = 0;
  for (size_t i = 0; ok && i < formal.length; i++) {
    Symbol *symbol;
    if (validate(formal.cells[i], V_SYMBOL(&symbol))) {
      if (symbol == key_keyword) {
        key = 1;
        break;
      } else if (symbol == opt_keyword) {
        for ( ; i < formal.length; i++) {
          if (!syntax_is(formal.cells[i], VALUE_VECTOR)) {
            if (validate(formal.cells[i], V_SYMBOL(&symbol))) {
              if (symbol == key_keyword) {
                key = 1;
                break;
              } else if (symbol == rest_keyword) {
                variadic = 1;
                break;
              }
            } else {
              ok = 0;
              break;
            }
          }
          optional++;
        }
        break;
      } else if (symbol == rest_keyword) {
        variadic = 1;
        break;
      } else if (symbol == match_keyword) {
        i++;
      }
    } else {
      ok = 0;
    }
  }
  delete_slice(formal);
  if (!ok) {
    return NULL;
  }
  return get_func_type(min_arity, variadic || key || optional);
}
