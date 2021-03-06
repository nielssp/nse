#include "test.h"

#include "../src/runtime/hashmap.c"

void test_iterator() {
  Dictionary d = create_dictionary();
  dictionary_add(d, "key1", "value1");
  dictionary_add(d, "key2", "value2");
  dictionary_add(d, "key3", "value3");
  DictionaryIterator it = create_dictionary_iterator(d);
  int seen1 = 0;
  int seen2 = 0;
  int seen3 = 0;
  for (DictionaryEntry entry = dictionary_next(it); entry.key; entry = dictionary_next(it)) {
    if (strcmp(entry.key, "key1") == 0) {
      assert(!seen1);
      assert(strcmp(entry.value, "value1") == 0);
      seen1 = 1;
    } else if (strcmp(entry.key, "key2") == 0) {
      assert(!seen2);
      assert(strcmp(entry.value, "value2") == 0);
      seen2 = 1;
    } else if (strcmp(entry.key, "key3") == 0) {
      assert(!seen3);
      assert(strcmp(entry.value, "value3") == 0);
      seen3 = 1;
    } else {
      assert(0 && "Unknown key");
    }
  }
  assert(seen1);
  assert(seen2);
  assert(seen3);
  delete_dictionary_iterator(it);
  delete_dictionary(d);
}

int main() {
  run_test(test_iterator);
  return 0;
}

