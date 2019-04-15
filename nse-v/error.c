/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "value.h"
#include "error.h"
#include "module.h"

Symbol *out_of_memory_error = NULL;
Symbol *domain_error = NULL;
Symbol *pattern_error = NULL;
Symbol *name_error = NULL;
Symbol *io_error = NULL;
Symbol *syntax_error = NULL;

Module *error_module = NULL;

static char *alloc_error = "out of memory: could not allocate enough space for error message";
static char *error_string = NULL;
static Symbol *error_symbol = NULL;
Syntax *error_form = NULL;
size_t error_arg_index = -1;
List *stack_trace = NULL;

void init_error_module() {
  error_module = create_module("error");

  out_of_memory_error = module_extern_symbol_c(error_module, "out-of-memory-error");
  domain_error = module_extern_symbol_c(error_module, "domain-error");
  pattern_error = module_extern_symbol_c(error_module, "pattern-error");
  name_error = module_extern_symbol_c(error_module, "name-error");
  io_error = module_extern_symbol_c(error_module, "io-error");
  syntax_error = module_extern_symbol_c(error_module, "syntax-error");
}
void raise_error(Symbol *error_type, const char *format, ...) {
  va_list va;
  clear_error();
  char *buffer = malloc(50);
  if (!buffer) {
    error_string = alloc_error;
    error_symbol = TO_SYMBOL(copy_value(SYMBOL(out_of_memory_error)));
    return;
  }
  size_t size = 50;
  while (1) {
    va_start(va, format);
    int n = vsnprintf(buffer, size, format, va);
    va_end(va);
    if (n < 0) {
      error_string = alloc_error;
      error_symbol = TO_SYMBOL(copy_value(SYMBOL(out_of_memory_error)));
      break;
    }
    if (n < size) {
      error_string = buffer;
      error_symbol = TO_SYMBOL(copy_value(SYMBOL(error_type)));
      break;
    }
    size_t new_size = n + 1;
    char *new_buffer = malloc(new_size);
    if (!new_buffer) {
      error_string = alloc_error;
      error_symbol = TO_SYMBOL(copy_value(SYMBOL(out_of_memory_error)));
      break;
    }
    memcpy(new_buffer, buffer, size);
    free(buffer);
    buffer = new_buffer;
    size = new_size;
  }
}

const char *current_error() {
  return error_string;
}

Symbol *current_error_type() {
  return error_symbol;
}

void clear_error() {
  if (error_string) {
    if (error_string != alloc_error) {
      free(error_string);
    }
    error_string = NULL;
  }
  if (error_symbol) {
    delete_value(SYMBOL(error_symbol));
    error_symbol = NULL;
  }
}

void *allocate(size_t bytes) {
  void *p = malloc(bytes);
  if (!p) {
    raise_error(out_of_memory_error, "could not allocate %zd bytes of memory", bytes);
    return NULL;
  }
  return p;
}

void set_debug_form(Value form) {
  if (form.type == VALUE_SYNTAX) {
    if (error_form) {
      delete_value(SYNTAX(error_form));
    }
    error_form = TO_SYNTAX(form);
  } else {
    delete_value(form);
  }
  error_arg_index = -1;
}

void set_debug_arg_index(size_t index) {
  error_arg_index = index;
}

Syntax *push_debug_form(Value syntax) {
  Syntax *previous = error_form;
  if (syntax.type == VALUE_SYNTAX) {
    error_form = TO_SYNTAX(syntax);
  }
  error_arg_index = -1;
  return previous;
}

Value pop_debug_form(Value result, Syntax *previous) {
  if (!RESULT_OK(result)) {
    if (previous) {
      delete_value(SYNTAX(previous));
    }
    return result;
  }
  if (error_form) {
    delete_value(SYNTAX(error_form));
  }
  error_form = previous;
  return result;
}

int stack_trace_push(Value func, Value args) {
  if (!error_form) {
    delete_value(func);
    delete_value(args);
    return 1;
  }
  Vector *v = create_vector(3);
  if (!v) {
    delete_value(func);
    delete_value(args);
    return 0;
  }
  v->cells[0] = func;
  v->cells[1] = args;
  v->cells[2] = SYNTAX(error_form);
  if (stack_trace) {
    List *new_trace = create_list(VECTOR(v), copy_object(stack_trace));
    if (!new_trace) {
      return 0;
    }
    delete_value(LIST(stack_trace));
    stack_trace = new_trace;
    return 1;
  } else {
    stack_trace = create_list(VECTOR(v), NULL);
    return !!stack_trace;
  }
}

void stack_trace_pop() {
  if (stack_trace) {
    List *old = stack_trace;
    if (old->tail) {
      stack_trace = copy_object(old->tail);
    } else {
      stack_trace = NULL;
    }
    delete_value(LIST(old));
  }
}

List *get_stack_trace() {
  return stack_trace;
}

void clear_stack_trace() {
  if (stack_trace) {
    delete_value(LIST(stack_trace));
    stack_trace = NULL;
  }
}
