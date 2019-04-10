#include <stdio.h>
#include <stdlib.h>

#include "value.h"

Value undefined = (Value){ .type = TYPE_UNDEFINED, .object = NULL };
Value unit = (Value){ .type = TYPE_UNIT, .object = NULL };

Vector *create_vector(size_t length) {
  Object *object = malloc(sizeof(Object) + sizeof(Vector) + sizeof(Value) * length);
  object->refs = 1;
  Vector *vector = (Vector *)object->data;
  vector->length = length;
  for (int i = 0; i < length; i++) {
    vector->cells[i] = undefined;
  }
  return vector;
}

VectorSlice *create_vector_slice(Vector *parent, size_t offset, size_t length) {
  Object *object = malloc(sizeof(Object) + sizeof(VectorSlice));
  object->refs = 1;
  VectorSlice *vector_slice = (VectorSlice *)object->data;
  vector_slice->length = length;
  vector_slice->vector = parent;
  vector_slice->cells = parent->cells + offset;;
  return vector_slice;
}

Value copy_value(Value val) {
  if (val.type & TYPE_OBJECT) {
    val.object->refs++;
  }
  return val;
}

void delete_value(Value val) {
  if (!(val.type & TYPE_OBJECT)) {
    return;
  }
  if (val.object->refs > 0) {
    val.object->refs--;
  }
  if (val.object->refs == 0) {
    switch (val.type) {
      case TYPE_VECTOR: {
        Vector *vector = TO_VECTOR(val);
        for (int i = 0; i < vector->length; i++) {
          delete_value(vector->cells[i]);
        }
        break;
      }
      case TYPE_VECTOR_SLICE:
        delete_value(VECTOR(TO_VECTOR_SLICE(val)->vector));
        break;
      default:
        break;
    }
    free(val.object);
  }
}

int main() {
  Vector *v = create_vector(2);
  v->cells[0] = I64(1337);
  v->cells[1] = VECTOR(create_vector(0));
  delete_value(VECTOR(v));
  v = NULL;
  return 0;
}
