#include "test.h"

#include "read.c"

Syntax *parse_int_string(const char *string) {
  Stack *stack = open_stack_string(string);
  Syntax *result = parse_int(stack);
  close_stack(stack);
  return result;
}

void test_parse_int() {
  Syntax *result = parse_int_string("15");
  assert(result->start_line = 1);
  assert(result->start_column = 1);
  assert(result->end_line = 1);
  assert(result->end_column = 3);
  NseVal value = result->quoted;
  assert(value.type == TYPE_I64);
  assert(value.i64 == 15);
  free(result);

  result = parse_int_string("-125215");
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
  run_test(test_parse_int);
  return 0;
}

