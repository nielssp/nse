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
