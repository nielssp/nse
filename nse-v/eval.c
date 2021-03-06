/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "module.h"
#include "error.h"
#include "lang.h"
#include "special.h"
#include "type.h"
#include "write.h"

#include "eval.h"

Value apply_generic(GenFunc *func, Slice args, Scope *dynamic_scope) {
  if (!func->context) {
    delete_value(GEN_FUNC(func));
    delete_slice(args);
    raise_error(name_error, "generic function has no methods in the current module");
    return undefined;
  }
  if (args.length < func->min_arity) {
    delete_value(GEN_FUNC(func));
    delete_slice(args);
    raise_error(domain_error, "expected at least %d parameters", func->min_arity);
    return undefined;
  }
  TypeArray *types = create_type_array_null(func->type_parameters);
  for (int i = 0; i < func->min_arity; i++) {
    int index = func->parameter_indices[i];
    if (index >= 0) {
      if (!types->elements[index]) {
        types->elements[index] = get_type(args.cells[i]);
      } else {
        types->elements[index] = unify_types(types->elements[index],
            get_type(args.cells[i]));
      }
    }
  }
  if (func->variadic) {
    int v_index = func->parameter_indices[func->min_arity];
    if (v_index >= 0) {
      for (int i = func->min_arity; i < args.length; i++) {
        if (!types->elements[v_index]) {
          types->elements[v_index] = get_type(args.cells[i]);
        } else {
          types->elements[v_index] = unify_types(types->elements[v_index],
              get_type(args.cells[i]));
        }
      }
    }
  }
  for (int i = 0; i < types->size; i++) {
    if (!types->elements[i]) {
      types->elements[i] = copy_type(nothing_type);
    }
  }
  Value method = module_find_method(func->context, func->name, types);
  delete_value(GEN_FUNC(func));
  if (!RESULT_OK(method)) {
    char *s = write_type_array_to_string(types, NULL);
    raise_error(name_error, "no method matching types (%s) found", s);
    free(s);
    delete_slice(args);
    delete_type_array(types);
    return undefined;
  }
  delete_type_array(types);
  return apply(method, args, dynamic_scope);
}

Value apply(Value function, Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  Syntax *old_error_form = error_form;
  switch (function.type) {
    case VALUE_FUNC:
      if (!stack_trace_push(copy_value(function), copy_slice(args))) {
        delete_slice(args);
      } else {
        result = function.func(args, dynamic_scope);
      }
      break;
    case VALUE_CLOSURE:
      if (!stack_trace_push(copy_value(function), copy_slice(args))) {
        delete_slice(args);
      } else {
        result = TO_CLOSURE(function)->f(args, TO_CLOSURE(function),
            dynamic_scope);
      }
      break;
    case VALUE_GEN_FUNC:
      if (!stack_trace_push(copy_value(function), copy_slice(args))) {
        delete_slice(args);
      } else {
        result = apply_generic(copy_object(TO_GEN_FUNC(function)), args, dynamic_scope);
      }
      break;
    case VALUE_VECTOR:
      if (args.length == 1 && args.cells[0].type == VALUE_I64) {
        Vector *v = TO_VECTOR(function);
        int64_t index = args.cells[0].i64;
        if (index >= 0 && index < v->length) {
          result = copy_value(v->cells[index]);
        } else {
          raise_error(domain_error, "index out of bounds");
        }
      } else {
        raise_error(domain_error, "expected (VECTOR INDEX)");
      }
      delete_slice(args);
      break;
    default:
      delete_slice(args);
      raise_error(domain_error, "not a function");
  }
  delete_value(function);
  if (RESULT_OK(result)) {
    if (old_error_form) {
      stack_trace_pop();
    }
  }
  return result;
}

Slice eval_args(Slice args, Scope *scope) {
  Vector *values = create_vector(args.length);
  if (!values) {
    delete_slice(args);
    return SLICE_ERROR;
  }
  for (size_t i = 0; i < args.length; i++) {
    values->cells[i] = eval(copy_value(args.cells[i]), scope);
    if (!RESULT_OK(values->cells[i])) {
      delete_slice(args);
      delete_value(VECTOR(values));
      return SLICE_ERROR;
    }
  }
  delete_slice(args);
  return to_slice(VECTOR(values));
}

Value eval_block(Slice block, Scope *scope) {
  Value result = unit;
  Scope *current_scope = scope;
  for (size_t i = 0; i < block.length; i++) {
    delete_value(result);
    Value statement = block.cells[i];
    if (syntax_is(statement, VALUE_VECTOR)) {
      Vector *v = TO_VECTOR(syntax_get(statement));
      if (v->length == 3 && syntax_equals(v->cells[0], SYMBOL(let_symbol)) == EQ_EQUAL && syntax_is(v->cells[1], VALUE_SYMBOL)) {
        Value value = eval(copy_value(v->cells[2]), current_scope);
        if (!RESULT_OK(value)) {
          result = undefined;
          break;
        }
        current_scope = scope_push(current_scope, TO_SYMBOL(copy_value(syntax_get(v->cells[1]))), value);
        continue;
      }
    }
    result = eval(copy_value(statement), current_scope);
    if (!RESULT_OK(result)) {
      break;
    }
  }
  scope_pop_until(current_scope, scope);
  delete_slice(block);
  return result;
}

Value eval_slice(Slice slice, Scope *scope) {
  if (slice.length == 0) {
    delete_slice(slice);
    return unit;
  }
  Value operator = copy_value(slice.cells[0]);
  Slice args = slice_slice(slice, 1, slice.length - 1);
  if (syntax_is(operator, VALUE_SYMBOL)) {
    Symbol *s = TO_SYMBOL(syntax_get(operator));
    Value macro = scope_get_macro(scope, copy_object(s));
    if (RESULT_OK(macro)) {
      delete_value(operator);
      Value expanded = apply(macro, args, scope);
      return THEN(expanded, eval(expanded, scope));
    }
    Value special = scope_get_in_namespace(scope, copy_object(s), eval_namespace);
    if (RESULT_OK(special)) {
      delete_value(operator);
      return apply(special, args, scope);
    }
  }
  Value result = undefined;
  Value function = eval(operator, scope);
  if (RESULT_OK(function)) {
    Slice arg_values = eval_args(args, scope);
    if (SLICE_OK(arg_values)) {
      result = apply(function, arg_values, scope);
      if (!RESULT_OK(result) && error_arg_index >= 0) {
        if (error_arg_index < args.length) {
          set_debug_form(copy_value(args.cells[error_arg_index]));
        } else {
          set_debug_arg_index(-1);
        }
      }
    } else {
      delete_value(function);
    }
  } else {
    delete_slice(args);
  }
  return result;
}

Value eval(Value code, Scope *scope) {
  switch (code.type) {
    case VALUE_I64:
    case VALUE_F64:
    case VALUE_STRING:
    case VALUE_KEYWORD:
    case VALUE_UNDEFINED:
      return code;
    case VALUE_VECTOR:
    case VALUE_VECTOR_SLICE:
      return eval_slice(to_slice(code), scope);
    case VALUE_SYMBOL: {
      Value value = scope_get(scope, TO_SYMBOL(code));
      if (RESULT_OK(value) && value.type == VALUE_GEN_FUNC
          && !TO_GEN_FUNC(value)->context) {
        GenFunc *gf = TO_GEN_FUNC(value);
        Value gf_copy = check_alloc(GEN_FUNC(create_gen_func(
                copy_object(gf->name), scope->module, gf->min_arity,
                gf->variadic, gf->type_parameters, gf->parameter_indices)));
        delete_value(value);
        return gf_copy;
      }
      return value;
    }
    case VALUE_SYNTAX: {
      Syntax *previous = push_debug_form(copy_value(code));
      Value result = eval(copy_value(TO_SYNTAX(code)->quoted), scope);
      delete_value(code);
      return pop_debug_form(result, previous);
    }
    default:
      raise_error(domain_error, "unexpected %s", value_type_name(code.type));
      delete_value(code);
      return undefined;
  }
}

static Value macro_expand_slice(Slice slice, Scope *scope) {
  if (slice.length == 0) {
    return slice_to_value(slice);
  }
  if (syntax_is(slice.cells[0], VALUE_SYMBOL)) {
    Symbol *s = TO_SYMBOL(syntax_get(slice.cells[0]));
    Value macro = scope_get_macro(scope, copy_object(s));
    if (RESULT_OK(macro)) {
      Slice args = slice_slice(slice, 1, slice.length - 1);
      Value expanded = apply(macro, args, scope);
      return THEN(expanded, macro_expand(expanded, scope));
    }
  }
  Vector *expanded = create_vector(slice.length);
  if (!expanded) {
    delete_slice(slice);
    return undefined;
  }
  for (int i = 0; i < slice.length; i++) {
    expanded->cells[i] = macro_expand(copy_value(slice.cells[i]), scope);
    if (!RESULT_OK(expanded->cells[i])) {
      delete_slice(slice);
      delete_value(VECTOR(expanded));
      return undefined;
    }
  }
  delete_slice(slice);
  return VECTOR(expanded);
}


Value macro_expand(Value code, Scope *scope) {
  switch (code.type) {
    case VALUE_VECTOR:
    case VALUE_VECTOR_SLICE:
      return macro_expand_slice(to_slice(code), scope);
    case VALUE_SYNTAX: {
      Syntax *previous = push_debug_form(copy_value(code));
      Value result = macro_expand(copy_value(TO_SYNTAX(code)->quoted), scope);
      delete_value(code);
      result = check_alloc(SYNTAX(copy_syntax(result, copy_object(previous))));
      return pop_debug_form(result, previous);
    }
    default:
      return code;
  }
}
