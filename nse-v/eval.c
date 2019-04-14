/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */

#include "value.h"
#include "module.h"
#include "error.h"

Value apply(Value function, Value args) {
  Value result = undefined;
  Syntax *old_error_form = error_form;
  switch (function.type) {
    case VALUE_FUNC:
      if (!stack_trace_push(copy_value(function), copy_value(args))) {
        return undefined;
      }
      result = function.func(args);
      delete_value(function);
      break;
    case VALUE_CLOSURE:
      if (!stack_trace_push(copy_value(function), copy_value(args))) {
        return undefined;
      }
      result = TO_CLOSURE(function)->f(args, TO_CLOSURE(function));
      break;
    default:
      delete_value(function);
      delete_value(args);
      raise_error(domain_error, "not a function");
  }
  if (RESULT_OK(result)) {
    if (old_error_form) {
      stack_trace_pop();
    }
  }
  return result;
}

Value eval(Value code, Scope *scope) {
  switch (code.type) {
    case VALUE_I64:
    case VALUE_F64:
    case VALUE_STRING:
    case VALUE_KEYWORD:
      return code;
    case VALUE_QUOTE: {
      Value quoted = copy_value(TO_QUOTE(code)->quoted);
      delete_value(code);
      return syntax_to_datum(quoted);
    }
    case VALUE_SYMBOL:
      return scope_get(scope, TO_SYMBOL(code));
    case VALUE_SYNTAX: {
      Syntax *previous = push_debug_form(TO_SYNTAX(copy_value(code)));
      Value result = eval(copy_value(TO_SYNTAX(code)->quoted), scope);
      delete_value(code);
      return pop_debug_form(result, previous);
    }
    default:
      raise_error(domain_error, "unexpected %s", value_type_name(code.type), code.type);
      delete_value(code);
      return undefined;
  }
}
