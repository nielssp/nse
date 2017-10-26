#include "test.h"

#include "type.c"

static void assert_simple_type(Type *a, Type *b) {
  assert(is_subtype_of(a, b));
  assert(is_subtype_of(a, a));
  assert(is_subtype_of(b, b));
  assert(is_subtype_of(a, any_type));
}

static void test_simple_is_subtype_of() {
  assert(is_subtype_of(nil_type, nil_type));
  assert(is_subtype_of(nil_type, any_type));
  assert(is_subtype_of(string_type, string_type));
  assert(is_subtype_of(string_type, any_type));
  assert(is_subtype_of(type_type, type_type));
  assert(is_subtype_of(type_type, any_type));
  assert(is_subtype_of(type_type, type_type));
  assert(is_subtype_of(type_type, any_type));
  assert_simple_type(i8_type, i16_type);
  assert_simple_type(i8_type, i32_type);
  assert_simple_type(i8_type, i64_type);
  assert_simple_type(i16_type, i32_type);
  assert_simple_type(i16_type, i64_type);
  assert_simple_type(i32_type, i64_type);
  assert_simple_type(u8_type, u16_type);
  assert_simple_type(u8_type, u32_type);
  assert_simple_type(u8_type, u64_type);
  assert_simple_type(u16_type, u32_type);
  assert_simple_type(u16_type, u64_type);
  assert_simple_type(u32_type, u64_type);
  assert_simple_type(u8_type, i16_type);
  assert_simple_type(u8_type, i32_type);
  assert_simple_type(u8_type, i64_type);
  assert_simple_type(u16_type, i32_type);
  assert_simple_type(u16_type, i64_type);
  assert_simple_type(u32_type, i64_type);
  assert_simple_type(f32_type, f64_type);
}

static void test_symbol_subtype() {
  Type *t1 = create_symbol_type("t");
  Type *t2 = create_symbol_type("f");
  assert(is_subtype_of(t1, any_type));
  assert(is_subtype_of(t1, any_symbol_type));
  assert(is_subtype_of(t1, t1));
  assert(!is_subtype_of(t1, t2));
  delete_type(t1);
  delete_type(t2);
}

static void test_union_subtype() {
  Type *t1 = create_symbol_type("foo");
  Type *t2 = create_symbol_type("bar");
  Type *t3 = create_symbol_type("baz");
  Type *t4 = create_symbol_type("bar");
  Type *t5 = create_union_type(copy_type(t1), copy_type(t2));
  assert(is_subtype_of(t1, t5));
  assert(is_subtype_of(t2, t5));
  assert(is_subtype_of(t4, t5));
  assert(!is_subtype_of(t5, t1));
  assert(!is_subtype_of(t5, t2));
  assert(!is_subtype_of(t5, t3));
  assert(!is_subtype_of(t3, t5));
  delete_type(t1);
  delete_type(t2);
  delete_type(t3);
  delete_type(t4);
  delete_type(t5);
}

static void test_recur_subtype() {
  Type *t1 = create_cons_type(copy_type(i64_type), create_type_var("r"));
  Type *t2 = create_union_type(copy_type(t1), copy_type(nil_type));
  Type *t3 = create_recur_type("r", copy_type(t2));

  Type *t4 = create_cons_type(copy_type(i64_type), copy_type(nil_type));
  Type *t5 = create_cons_type(copy_type(i64_type), copy_type(t4));
  Type *t6 = create_cons_type(copy_type(i64_type), copy_type(t5));

  Type *t7 = create_cons_type(copy_type(string_type), copy_type(t4));
  Type *t8 = create_cons_type(copy_type(i64_type), copy_type(t7));

  assert(is_subtype_of(nil_type, t3));
  assert(is_subtype_of(t4, t3));
  assert(is_subtype_of(t5, t3));
  assert(is_subtype_of(t6, t3));

  assert(!is_subtype_of(i64_type, t3));
  assert(!is_subtype_of(t7, t3));
  assert(!is_subtype_of(t8, t3));

  delete_type(t1);
  delete_type(t2);
  delete_type(t3);
  delete_type(t4);
  delete_type(t5);
  delete_type(t6);
  delete_type(t7);
  delete_type(t8);
}

int main() {
  run_test(test_simple_is_subtype_of);
  run_test(test_symbol_subtype);
  run_test(test_union_subtype);
  run_test(test_recur_subtype);
  return 0;
}
