#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "error.h"

static char *alloc_error = "out of memory: could not allocate enough space for error message";
static char *error_string = NULL;

void raise_error(const char *format, ...) {
  va_list va;
  clear_error();
  char *buffer = malloc(50);
  if (!buffer) {
    error_string = alloc_error;
    return;
  }
  size_t size = 50;
  while (1) {
    va_start(va, format);
    int n = vsnprintf(buffer, size, format, va);
    va_end(va);
    if (n < 0) {
      error_string = alloc_error;
      break;
    }
    if (n  < size) {
      error_string = buffer;
      break;
    }
    size_t new_size = n + 1;
    char *new_buffer = malloc(new_size);
    if (!new_buffer) {
      error_string = alloc_error;
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

void clear_error() {
  if (error_string) {
    if (error_string != alloc_error) {
      free(error_string);
    }
    error_string = NULL;
  }
}

void *allocate(size_t bytes) {
  void *p = malloc(bytes);
  if (!p) {
    raise_error("out of memory: could not allocate %zd bytes of memory", bytes);
    return NULL;
  }
  return p;
}
