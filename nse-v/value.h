#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>

#define I64(i) ((Value){ .type = TYPE_I64, .i64 = (i) })
#define VECTOR(v) ((Value){ .type = TYPE_VECTOR, .object = ((Object *)(v) - 1) })
#define VECTOR_SLICE(v) ((Value){ .type = TYPE_VECTOR_SLICE, .object = ((Object *)(v) - 1) })

#define TO_VECTOR(val) ((Vector *)(val).object->data)
#define TO_VECTOR_SLICE(val) ((VectorSlice *)(val).object->data)

typedef struct Value Value;
typedef struct Object Object;
typedef struct Vector Vector;
typedef struct VectorSlice VectorSlice;

typedef enum {
  TYPE_UNDEFINED    = 0x00,
  TYPE_UNIT         = 0x01,
  TYPE_I64          = 0x02,
  TYPE_OBJECT       = 0x10,
  TYPE_VECTOR       = 0x11,
  TYPE_VECTOR_SLICE = 0x12
} Type;

struct Value {
  Type type;
  union {
    int64_t i64;
    Object *object;
  };
};

struct Vector {
  size_t length;
  Value cells[];
};

struct VectorSlice {
  size_t length;
  Vector *vector;
  Value *cells;
};

struct Object {
  size_t refs;
  uint8_t data[];
};

Vector *create_vector(size_t length);

VectorSlice *create_vector_slice(Vector *parent, size_t offset, size_t length);

Value copy_value(Value val);

void delete_value(Value val);

#endif
