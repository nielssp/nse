#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "nsert.h"
#include "error.h"

Symbol *out_of_memory_error = NULL;
Symbol *domain_error = NULL;
Symbol *name_error = NULL;
Symbol *io_error = NULL;
Symbol *syntax_error = NULL;

Module *error_module = NULL;

static char *alloc_error = "out of memory: could not allocate enough space for error message";
static char *error_string = NULL;
static Symbol *error_symbol = NULL;

void init_error_module() {
  error_module = create_module("error");

  out_of_memory_error = module_extern_symbol(error_module, "out-of-memory-error");
  domain_error = module_extern_symbol(error_module, "domain-error");
  name_error = module_extern_symbol(error_module, "name-error");
  io_error = module_extern_symbol(error_module, "io-error");
  syntax_error = module_extern_symbol(error_module, "syntax-error");
}

void raise_error(Symbol *error_type, const char *format, ...) {
  va_list va;
  clear_error();
  char *buffer = malloc(50);
  if (!buffer) {
    error_string = alloc_error;
    error_symbol = add_ref(SYMBOL(out_of_memory_error)).symbol;
    return;
  }
  size_t size = 50;
  while (1) {
    va_start(va, format);
    int n = vsnprintf(buffer, size, format, va);
    va_end(va);
    if (n < 0) {
      error_string = alloc_error;
      error_symbol = add_ref(SYMBOL(out_of_memory_error)).symbol;
      break;
    }
    if (n < size) {
      error_string = buffer;
      error_symbol = add_ref(SYMBOL(error_type)).symbol;
      break;
    }
    size_t new_size = n + 1;
    char *new_buffer = malloc(new_size);
    if (!new_buffer) {
      error_string = alloc_error;
      error_symbol = add_ref(SYMBOL(out_of_memory_error)).symbol;
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
    del_ref(SYMBOL(error_symbol));
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
