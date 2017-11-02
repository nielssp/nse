#include "test.h"

#include "read.c"

#include <string.h>

Syntax *read_int_string(const char *string) {
  Module *module = create_module("test");
  Stream *stream = stream_string(string);
  Reader *reader = open_reader(stream, "test", module);
  Syntax *result = read_int(reader);
  close_reader(reader);
  delete_module(module);
  return result;
}

void test_read_int() {
  Syntax *result = read_int_string("15");
  assert(result->start_line = 1);
  assert(result->start_column = 1);
  assert(result->end_line = 1);
  assert(result->end_column = 3);
  NseVal value = result->quoted;
  assert(value.type == TYPE_I64);
  assert(value.i64 == 15);
  free(result);

  result = read_int_string("-125215");
  assert(result->start_line = 1);
  assert(result->start_column = 1);
  assert(result->end_line = 1);
  assert(result->end_column = 8);
  value = result->quoted;
  assert(value.type == TYPE_I64);
  assert(value.i64 == -125215);
  free(result);
}

int main() {
  run_test(test_read_int);
  return 0;
}

