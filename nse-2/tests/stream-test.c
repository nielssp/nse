#include "test.h"

#include "util/stream.c"

void test_buffer_stream() {
  char *buf = malloc(5);
  Stream *s = stream_buffer(buf, 5);
  assert(stream_get_size(s) == 5);
  stream_printf(s, "1234");
  assert(stream_get_size(s) == 5);
  stream_printf(s, "123456");
  assert(stream_get_size(s) >= 10);
}

int main() {
  run_test(test_buffer_stream);
  return 0;
}

