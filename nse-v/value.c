/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type.h"
#include "error.h"
#include "value.h"

/* Primitives */

Value undefined = (Value){ .type = VALUE_UNDEFINED, .object = NULL };
Value unit = (Value){ .type = VALUE_UNIT, .object = NULL };

const char *value_type_name(ValueType type) {
  switch (type) {
    case VALUE_UNDEFINED: return "undefined";
    case VALUE_UNIT: return "unit";
    case VALUE_I64: return "i64";
    case VALUE_F64: return "f64";
    case VALUE_FUNC: return "func";

    case VALUE_VECTOR: return "vector";
    case VALUE_VECTOR_SLICE: return "vector-slice";
    case VALUE_ARRAY: return "array";
    case VALUE_ARRAY_SLICE: return "array-slice";
    case VALUE_ARRAY_BUFFER: return "array-buffer";
    case VALUE_LIST: return "list";
    case VALUE_STRING: return "string";
    case VALUE_WEAK_REF: return "weak-ref";
    case VALUE_SYMBOL: return "symbol";
    case VALUE_KEYWORD: return "keyword";
    case VALUE_DATA: return "data";
    case VALUE_SYNTAX: return "syntax";
    case VALUE_CLOSURE: return "closure";
    case VALUE_POINTER: return "pointer";
    case VALUE_TYPE: return "type";
    case VALUE_GEN_FUNC: return "gen-func";
    case VALUE_HASH_MAP: return "hash-map";
    default: return "???";
  }
}

static Equality cells_equal(const Value *a, size_t a_length, const Value *b, size_t b_length) {
  if (a_length != b_length) {
    return EQ_NOT_EQUAL;
  }
  for (size_t i = 0; i < a_length; i++) {
    Equality e = equals(a[i], b[i]);
    if (e != EQ_EQUAL) {
      return e;
    }
  }
  return EQ_EQUAL;
}

Equality equals(const Value a, const Value b) {
  if (a.type == VALUE_UNDEFINED || b.type == VALUE_UNDEFINED) {
    return EQ_ERROR;
  }
  if (a.type != b.type) {
    return EQ_NOT_EQUAL;
  }
  if (a.type & VALUE_OBJECT && a.object == b.object) {
    return EQ_EQUAL;
  }
  switch (a.type) {
    case VALUE_UNDEFINED:
      return EQ_ERROR;
    case VALUE_UNIT:
      return EQ_EQUAL;
    case VALUE_I64:
      return B_TO_EQ(a.i64 == b.i64);
    case VALUE_F64:
      return B_TO_EQ(a.f64 == b.f64);
    case VALUE_FUNC:
      return EQ_NOT_EQUAL;


    case VALUE_VECTOR:
    case VALUE_ARRAY: {
      Vector *va = TO_VECTOR(a);
      Vector *vb = TO_VECTOR(b);
      return cells_equal(va->cells, va->length, vb->cells, vb->length);
    }
    case VALUE_VECTOR_SLICE:
    case VALUE_ARRAY_SLICE: {
      VectorSlice *va = TO_VECTOR_SLICE(a);
      VectorSlice *vb = TO_VECTOR_SLICE(b);
      return cells_equal(va->cells, va->length, vb->cells, vb->length);
    }
    case VALUE_ARRAY_BUFFER: {
      ArrayBuffer *aa = TO_ARRAY_BUFFER(a);
      ArrayBuffer *ab = TO_ARRAY_BUFFER(b);
      return cells_equal(aa->cells, aa->length, ab->cells, ab->length);
    }
    case VALUE_LIST: {
      List *la = TO_LIST(a);
      List *lb = TO_LIST(b);
      while (la || lb) {
        if (!la || !lb) {
          return EQ_NOT_EQUAL;
        }
        Equality r = equals(la->head, lb->head);
        if (r != EQ_EQUAL) {
          return r;
        }
      }
      return EQ_EQUAL;
    }
    case VALUE_STRING: {
      String *sa = TO_STRING(a);
      String *sb = TO_STRING(b);
      if (sa->length != sb->length) {
        return EQ_NOT_EQUAL;
      }
      return B_TO_EQ(memcmp(sa->bytes, sb->bytes, sa->length) == 0);
    }
    case VALUE_WEAK_REF:
      return equals(TO_WEAK_REF(a)->value, TO_WEAK_REF(b)->value);
    case VALUE_DATA: {
      Data *da = TO_DATA(a);
      Data *db = TO_DATA(b);
      if (da == db) {
        return EQ_EQUAL;
      }
      if (da->type != db->type) {
        return EQ_NOT_EQUAL;
      }
      if (da->tag != db->tag) {
        return EQ_NOT_EQUAL;
      }
      return cells_equal(da->fields, da->size, db->fields, db->size);
    }
    case VALUE_SYNTAX: {
      // TODO
      return EQ_NOT_EQUAL;
    }
    case VALUE_CLOSURE:
      return EQ_NOT_EQUAL;
    case VALUE_HASH_MAP: {
      HashMap *map_a = TO_HASH_MAP(a);
      HashMap *map_b = TO_HASH_MAP(b);
      HashMapEntry entry_a;
      HashMapIterator it = generic_hash_map_iterate(&map_a->map);
      while (generic_hash_map_next(&it, &entry_a)) {
        HashMapEntry entry_b;
        if (generic_hash_map_get(&map_b->map, &entry_a, &entry_b)) {
          Equality e = equals(entry_a.key, entry_b.key);
          if (e != EQ_EQUAL) {
            return e;
          }
          e = equals(entry_a.value, entry_b.value);
          if (e != EQ_EQUAL) {
            return e;
          }
        } else {
          return EQ_NOT_EQUAL;
        }
      }
      return EQ_EQUAL;
    }
    default:
      return EQ_NOT_EQUAL;
  }
}

/* Object allocation */

void *allocate_object(size_t size) {
  Object *object = allocate(size);
  if (!object) {
    return NULL;
  }
  object->refs = 1;
  object->weak_refs = NULL;
  return object;
}

Value copy_value(Value val) {
  if (val.type & VALUE_OBJECT) {
    val.object->refs++;
  }
  return val;
}

void *copy_object(void *object) {
  ((Object *)object)->refs++;
  return object;
}

void delete_value(Value val) {
  if (!(val.type & VALUE_OBJECT)) {
    return;
  } else if (val.type == VALUE_TYPE) {
    delete_type(TO_TYPE(val));
    return;
  }
  if (val.object->refs > 0) {
    val.object->refs--;
  }
  if (val.object->refs == 0) {
    if (val.object->weak_refs) {
      for (WeakRef *weak = val.object->weak_refs; weak; weak = weak->next) {
        weak->value = unit;
      }
    }
    switch (val.type) {
      case VALUE_VECTOR:
      case VALUE_ARRAY: {
        Vector *vector = TO_VECTOR(val);
        for (int i = 0; i < vector->length; i++) {
          delete_value(vector->cells[i]);
        }
        delete_type(vector->type);
        break;
      }
      case VALUE_VECTOR_SLICE:
      case VALUE_ARRAY_SLICE:
        delete_value(VECTOR(TO_VECTOR_SLICE(val)->vector));
        delete_type(TO_VECTOR_SLICE(val)->type);
        break;
      case VALUE_ARRAY_BUFFER: {
        ArrayBuffer *buffer = TO_ARRAY_BUFFER(val);
        for (int i = 0; i < buffer->length; i++) {
          delete_value(buffer->cells[i]);
        }
        delete_type(TO_ARRAY_BUFFER(val)->type);
        free(buffer->cells);
        break;
      }
      case VALUE_LIST:
        delete_value(TO_LIST(val)->head);
        if (TO_LIST(val)->tail) {
          delete_value(LIST(TO_LIST(val)->tail));
        }
        break;
      case VALUE_STRING:
        break;
      case VALUE_WEAK_REF: {
        WeakRef *weak = TO_WEAK_REF(val);
        if (weak->value.type & VALUE_OBJECT) {
          if (weak == weak->value.object->weak_refs) {
            weak->value.object->weak_refs = weak->next;
          }
          if (weak->next) {
            weak->next->previous = weak->previous;
          }
          if (weak->previous) {
            weak->previous->next = weak->next;
          }
        }
        break;
      }
      case VALUE_SYMBOL:
      case VALUE_KEYWORD:
        delete_value(STRING(TO_SYMBOL(val)->name));
        break;
      case VALUE_DATA: {
        Data *d = TO_DATA(val);
        delete_type(d->type);
        delete_value(SYMBOL(d->tag));
        for (size_t i = 0; i < d->size; i++) {
          delete_value(d->fields[i]);
        }
        break;
      }
      case VALUE_CLOSURE: {
        Closure *c = TO_CLOSURE(val);
        for (size_t i = 0; i < c->env_size; i++) {
          delete_value(c->env[i]);
        }
        break;
      }
      case VALUE_POINTER: {
        Pointer *p = TO_POINTER(val);
        delete_type(p->type);
        if (p->destructor) {
          p->destructor(p->pointer);
        }
        break;
      }
      case VALUE_SYNTAX: {
        Syntax *s = TO_SYNTAX(val);
        if (s->file) {
          delete_value(STRING(s->file));
        }
        delete_value(s->quoted);
        break;
      }
      case VALUE_GEN_FUNC:
        delete_value(SYMBOL(TO_GEN_FUNC(val)->name));
        break;
      case VALUE_HASH_MAP: {
        HashMapEntry entry;
        HashMapIterator it = generic_hash_map_iterate(&TO_HASH_MAP(val)->map);
        while (generic_hash_map_next(&it, &entry)) {
          delete_value(entry.key);
          delete_value(entry.value);
        }
        delete_generic_hash_map(&TO_HASH_MAP(val)->map);
        break;
      }
      default:
        break;
    }
    free(val.object);
  }
}

Value check_alloc(Value v) {
  if (v.type & VALUE_OBJECT) {
    return v.object ? v : undefined;
  }
  return v;
}

/* Slices */

Slice to_slice(Value sequence) {
  Slice slice;
  slice.sequence = sequence;
  switch (sequence.type) {
    case VALUE_VECTOR:
    case VALUE_ARRAY: {
      Vector *v = TO_VECTOR(sequence);
      slice.length = v->length;
      slice.cells = v->cells;
      break;
    }
    case VALUE_VECTOR_SLICE: {
      VectorSlice *v = TO_VECTOR_SLICE(sequence);
      slice.sequence = copy_value(VECTOR(v->vector));
      slice.length = v->length;
      slice.cells = v->cells;
      delete_value(sequence);
      break;
    }
    case VALUE_ARRAY_SLICE: {
      ArraySlice *v = TO_ARRAY_SLICE(sequence);
      slice.sequence = copy_value(ARRAY(v->vector));
      slice.length = v->length;
      slice.cells = v->cells;
      delete_value(sequence);
      break;
    }
    default:
      slice.length = 1;
      slice.cells = &slice.sequence;
      break;
  }
  return slice;
}

size_t get_slice_length(const Value sequence) {
  switch (sequence.type) {
    case VALUE_VECTOR: 
    case VALUE_ARRAY: 
      return TO_VECTOR(sequence)->length;
    case VALUE_VECTOR_SLICE:
    case VALUE_ARRAY_SLICE:
      return TO_VECTOR_SLICE(sequence)->length;
    default:
      return 1;
  }
}

Slice slice(Value sequence, size_t offset, size_t length) {
  Slice slice;
  slice.sequence = sequence;
  slice.length = length;
  switch (sequence.type) {
    case VALUE_VECTOR:
    case VALUE_ARRAY: {
      Vector *v = TO_VECTOR(sequence);
      slice.cells = v->cells + offset;
      break;
    }
    case VALUE_VECTOR_SLICE: {
      VectorSlice *v = TO_VECTOR_SLICE(sequence);
      slice.sequence = copy_value(VECTOR(v->vector));
      slice.cells = v->cells + offset;
      delete_value(sequence);
      break;
    }
    case VALUE_ARRAY_SLICE: {
      ArraySlice *v = TO_ARRAY_SLICE(sequence);
      slice.sequence = copy_value(ARRAY(v->vector));
      slice.cells = v->cells + offset;
      delete_value(sequence);
      break;
    }
    default:
      slice.length = 1;
      slice.cells = &slice.sequence;
      break;
  }
  return slice;
}

Slice slice_slice(Slice slice, size_t offset, size_t length) {
  slice.cells += offset;
  slice.length = length;
  return slice;
}

Value slice_to_value(Slice slice) {
  switch (slice.sequence.type) {
    case VALUE_VECTOR: {
      Vector *v = TO_VECTOR(slice.sequence);
      if (slice.length == v->length && slice.cells == v->cells) {
        return VECTOR(v);
      }
      return VECTOR_SLICE(create_vector_slice(v, slice.cells - v->cells, slice.length));
    }
    case VALUE_VECTOR_SLICE: {
      VectorSlice *v = TO_VECTOR_SLICE(slice.sequence);
      if (slice.length == v->length && slice.cells == v->cells) {
        return VECTOR_SLICE(v);
      }
      return VECTOR_SLICE(slice_vector_slice(v, slice.cells - v->cells, slice.length));
    }
    case VALUE_ARRAY: {
      Array *v = TO_ARRAY(slice.sequence);
      if (slice.length == v->length && slice.cells == v->cells) {
        return ARRAY(v);
      }
      return ARRAY_SLICE(create_array_slice(v, slice.cells - v->cells, slice.length));
    }
    case VALUE_ARRAY_SLICE: {
      ArraySlice *v = TO_ARRAY_SLICE(slice.sequence);
      if (slice.length == v->length && slice.cells == v->cells) {
        return ARRAY_SLICE(v);
      }
      return ARRAY_SLICE(slice_array_slice(v, slice.cells - v->cells, slice.length));
    }
    default:
      return slice.sequence;
  }
}

Slice copy_slice(Slice slice) {
  copy_value(slice.sequence);
  return slice;
}

void delete_slice(Slice slice) {
  delete_value(slice.sequence);
}


/* Vector allocation */

Vector *create_vector(size_t length) {
  Vector *vector = allocate_object(sizeof(Vector) + sizeof(Value) * length);
  if (!vector) {
    return NULL;
  }
  vector->length = length;
  vector->type = NULL;
  for (int i = 0; i < length; i++) {
    vector->cells[i] = undefined;
  }
  return vector;
}

VectorBuilder create_vector_builder() {
  return (VectorBuilder){.size = 0, .vector = create_vector(0)};
}

VectorBuilder vector_builder_push(VectorBuilder builder, Value value) {
  if (builder.vector->length >= builder.size) {
    size_t new_size = (builder.size + 1) * 2;
    Vector *new_vector = realloc(builder.vector, sizeof(Vector) + sizeof(Value) * new_size);
    if (!new_vector) {
      raise_error(out_of_memory_error, "vector reallocation failed");
      delete_value(VECTOR(builder.vector));
      delete_value(value);
      builder.size = 0;
      builder.vector = NULL;
      return builder;
    }
    builder.size = new_size;
    builder.vector = new_vector;
  }
  builder.vector->cells[builder.vector->length] = value;
  builder.vector->length++;
  return builder;
}

/* Vector slice allocation */

VectorSlice *create_vector_slice(Vector *parent, size_t offset, size_t length) {
  VectorSlice *vector_slice = allocate_object(sizeof(VectorSlice));
  if (!vector_slice) {
    delete_value(VECTOR(parent));
    return NULL;
  }
  vector_slice->length = length;
  vector_slice->type = NULL;
  vector_slice->vector = parent;
  vector_slice->cells = parent->cells + offset;;
  return vector_slice;
}

VectorSlice *slice_vector_slice(VectorSlice *parent, size_t offset, size_t length) {
  if (offset == 0 && length == parent->length) {
    return parent;
  }
  VectorSlice *vector_slice = allocate_object(sizeof(VectorSlice));
  if (vector_slice) {
    vector_slice->length = length;
    vector_slice->vector = copy_object(parent->vector);
    vector_slice->cells = parent->cells + offset;;
  }
  delete_value(VECTOR_SLICE(parent));
  return vector_slice;
}

/* Array allocation */

Array *create_array(size_t length) {
  return create_vector(length);
}

Value array_set(Array *array, size_t index, Value value) {
  Value previous = array->cells[index];
  array->cells[index] = value;
  delete_value(ARRAY(array));
  return previous;
}

/* Array slice allocation */

ArraySlice *create_array_slice(Array *parent, size_t offset, size_t length) {
  return create_vector_slice(parent, offset, length);
}

ArraySlice *slice_array_slice(ArraySlice *parent, size_t offset, size_t length) {
  return slice_vector_slice(parent, offset, length);
}

Value array_slice_set(ArraySlice *array_slice, size_t index, Value value) {
  Value previous = array_slice->cells[index];
  array_slice->cells[index] = value;
  delete_value(ARRAY_SLICE(array_slice));
  return previous;
}

/* Array buffer allocation */

ArrayBuffer *create_array_buffer(size_t initial_size) {
  ArrayBuffer *buffer = allocate_object(sizeof(ArrayBuffer));
  if (!buffer) {
    return NULL;
  }
  buffer->size = initial_size;
  buffer->length = 0;
  buffer->type = NULL;
  if (buffer->size) {
    buffer->cells = allocate(sizeof(Value) * buffer->size);
    if (!buffer->cells) {
      free(buffer);
      return NULL;
    }
  } else {
    buffer->cells = NULL;
  }
  return buffer;
}

ArrayBuffer *slice_array_buffer(ArrayBuffer *buffer, size_t offset, size_t length) {
  ArrayBuffer *slice = create_array_buffer(length);
  if (!slice) {
    delete_value(ARRAY_BUFFER(buffer));
    return NULL;
  }
  slice->length = length;
  slice->type = copy_type(buffer->type);
  if (slice->size) {
    memcpy(slice->cells, buffer->cells + offset, sizeof(Value) * length);
    for (int i = 0; i < length; i++) {
      copy_value(slice->cells[i]);
    }
  }
  delete_value(ARRAY_BUFFER(buffer));
  return slice;
}

Value array_buffer_set(ArrayBuffer *buffer, size_t index, Value value) {
  Value previous = buffer->cells[index];
  buffer->cells[index] = value;
  delete_value(ARRAY_BUFFER(buffer));
  return previous;
}

Value array_buffer_delete(ArrayBuffer *buffer, size_t index) {
  Value previous = buffer->cells[index];
  size_t n = buffer->length - index - 1;
  if (n > 0) {
    memcpy(buffer->cells + index, buffer->cells + index + 1, sizeof(Value) * n);
  }
  buffer->length--;
  delete_value(ARRAY_BUFFER(buffer));
  return previous;
}

static int resize_array_buffer(ArrayBuffer *buffer) {
  if (buffer->length >= buffer->size) {
    size_t new_size = (buffer->size + 1) * 2;
    Value *new_cells = realloc(buffer->cells, sizeof(Value) * new_size);
    if (!new_cells) {
      raise_error(out_of_memory_error, "array buffer reallocation failed");
      return 0;
    }
    buffer->size = new_size;
    buffer->cells = new_cells;
  }
  return 1;
}

ArrayBuffer *array_buffer_push(ArrayBuffer *buffer, Value value) {
  if (!resize_array_buffer(buffer)) {
    delete_value(ARRAY_BUFFER(buffer));
    delete_value(value);
    return NULL;
  }
  buffer->cells[buffer->length] = value;
  buffer->length++;
  return buffer;
}

Value array_buffer_pop(ArrayBuffer *buffer) {
  buffer->length--;
  Value value = buffer->cells[buffer->length];
  delete_value(ARRAY_BUFFER(buffer));
  return value;
}

ArrayBuffer *array_buffer_insert(ArrayBuffer *buffer, size_t index, Value value) {
  if (!resize_array_buffer(buffer)) {
    delete_value(ARRAY_BUFFER(buffer));
    delete_value(value);
    return NULL;
  }
  size_t n = buffer->length - index;
  if (n > 0) {
    memcpy(buffer->cells + index + 1, buffer->cells + index, sizeof(Value) * n);
  }
  buffer->cells[index] = value;
  buffer->length++;
  return buffer;
}

/* List allocation */

List *create_list(Value head, List *tail) {
  List *list = allocate_object(sizeof(List));
  if (!list) {
    delete_value(head);
    delete_value(LIST(tail));
    return NULL;
  }
  list->head = head;
  list->tail = tail;
  return list;
}

ListBuilder create_list_builder() {
  return (ListBuilder){.first = NULL, .last = NULL};
}

ListBuilder list_builder_push(ListBuilder builder, Value value) {
  List *next = create_list(value, NULL);
  if (!next) {
    delete_value(LIST(builder.first));
    builder.first = builder.last = NULL;
    return builder;
  }
  if (builder.last) {
    builder.last->tail = next;
    builder.last = next;
  } else {
    builder.first = builder.last = next;
  }
  return builder;
}

/* String allocation */

String *create_string(const uint8_t *bytes, size_t length) {
  String *string = allocate_object(sizeof(String) + length + 1);
  if (!string) {
    return NULL;
  }
  string->length = length;
  memcpy(string->bytes, bytes, length);
  string->bytes[length] = 0;
  return string;
}

String *c_string_to_string(const char *str) {
  if (!str) {
    return NULL;
  }
  return create_string((const uint8_t *)str, strlen(str));
}

String *create_string_buffer(size_t capacity) {
  String *string = allocate_object(sizeof(String) + capacity);
  if (!string) {
    return NULL;
  }
  string->length = 0;
  string->bytes[0] = 0;
  return string;
}

String *resize_string_buffer(String *s, size_t new_capacity) {
  return realloc(s, sizeof(String) + new_capacity);
}

String *slice_string(String *string, size_t offset, size_t length) {
  String *slice = allocate_object(sizeof(String) + length + 1);
  if (!slice) {
    delete_value(STRING(string));
    return NULL;
  }
  slice->length = length;
  memcpy(slice->bytes, string->bytes + offset, length);
  slice->bytes[length] = 0;
  delete_value(STRING(string));
  return slice;
}

/* Weak reference allocation */

WeakRef *create_weak_ref(Value object) {
  WeakRef *ref = allocate_object(sizeof(WeakRef));
  if (!ref) {
    delete_value(object);
    return NULL;
  }
  ref->value = object;
  ref->type = NULL;
  ref->previous = NULL;
  if (object.type & VALUE_OBJECT) {
    ref->next = object.object->weak_refs;
    if (ref->next) {
      ref->next->previous = ref;
    }
    object.object->weak_refs = ref;
  } else {
    ref->next = NULL;
  }
  delete_value(object);
  return ref;
}

/* Symbol allocation */

Symbol *create_symbol(String *name, Module *module) {
  Symbol *symbol = allocate_object(sizeof(Symbol));
  if (!symbol) {
    delete_value(STRING(name));
    return NULL;
  }
  symbol->module = module;
  symbol->name = name;
  return symbol;
}

/* Data allocation */

Data *create_data(Type *type, Symbol *tag, Value const fields[], size_t size) {
  Data *data = allocate_object(sizeof(Data) + size * sizeof(Value));
  if (!data) {
    delete_type(type);
    delete_value(SYMBOL(tag));
    return NULL;
  }
  data->type = type;
  data->tag = tag;
  data->size = size;
  if (size > 0) {
    memcpy(data->fields, fields, size * sizeof(Value));
    for (size_t i = 0; i < size; i++) {
      copy_value(fields[i]);
    }
  }
  return data;
}

/* Closure allocation */

Closure *create_closure(ClosureFunc f, Value const env[], size_t env_size) {
  Closure *closure = allocate_object(sizeof(Closure) + env_size * sizeof(Value));
  if (!closure) {
    return NULL;
  }
  closure->f = f;
  closure->env_size = env_size;
  if (env_size > 0) {
    memcpy(closure->env, env, env_size * sizeof(Value));
    for (size_t i = 0; i < env_size; i++) {
      copy_value(env[i]);
    }
  }
  return closure;
}

/* Generic function allocaton */

GenFunc *create_gen_func(Symbol *name, Module *context, uint8_t min_arity, uint8_t variadic, uint8_t type_parameters, int8_t const parameter_indices[]) {
  GenFunc *gf = allocate_object(sizeof(GenFunc) + min_arity);
  if (!gf) {
    delete_value(SYMBOL(name));
    return NULL;
  }
  gf->name = name;
  gf->context = context;
  gf->min_arity = min_arity;
  gf->variadic = !!variadic;
  gf->type_parameters = type_parameters;
  if (gf->min_arity > 0 || gf->variadic) {
    memcpy(gf->parameter_indices, parameter_indices, min_arity + gf->variadic);
  }
  return gf;
}

/* Pointer allocation */

Pointer *create_pointer(Type *type, void *pointer, Destructor destructor) {
  Pointer *p = allocate_object(sizeof(Pointer));
  if (!p) {
    delete_type(type);
    destructor(pointer);
    return NULL;
  }
  p->type = type;
  p->pointer = pointer;
  p->destructor = destructor;
  return p;
}

void void_destructor(void *p) {
}

/* Syntax allocation */

Syntax *create_syntax(Value quoted) {
  Syntax *syntax = allocate_object(sizeof(Syntax));
  if (!syntax) {
    delete_value(quoted);
    return NULL;
  }
  syntax->start_line = 0;
  syntax->start_column = 0;
  syntax->end_line = 0;
  syntax->end_column = 0;
  syntax->file = NULL;
  syntax->quoted = quoted;
  return syntax;
}

Syntax *copy_syntax(Value quoted, Syntax *old) {
  Syntax *syntax = allocate_object(sizeof(Syntax));
  if (!syntax) {
    delete_value(quoted);
    delete_value(SYNTAX(old));
    return NULL;
  }
  syntax->start_line = old->start_line;
  syntax->start_column = old->start_column;
  syntax->end_line = old->end_line;
  syntax->end_column = old->end_column;
  if (old->file) {
    syntax->file = copy_object(old->file);
  } else {
    syntax->file = NULL;
  }
  syntax->quoted = quoted;
  delete_value(SYNTAX(old));
  return syntax;
}

Value syntax_to_datum(Value v) {
  Value result;
  switch (v.type) {
    case VALUE_SYNTAX:
      result = syntax_to_datum(copy_value(TO_SYNTAX(v)->quoted));
      delete_value(v);
      return result;
    case VALUE_VECTOR: {
      Vector *vector = TO_VECTOR(v);
      Vector *copy = create_vector(vector->length);
      for (int i = 0; i < vector->length; i++) {
        copy->cells[i] = syntax_to_datum(copy_value(vector->cells[i]));
      }
      delete_value(VECTOR(vector));
      return VECTOR(copy);
    }
    default:
      return v;
  }
}

int syntax_is(const Value syntax, ValueType type) {
  return syntax.type == type || (syntax.type == VALUE_SYNTAX && TO_SYNTAX(syntax)->quoted.type == type);
}

Equality syntax_equals(const Value syntax, const Value other) {
  if (syntax.type == VALUE_SYNTAX) {
    return equals(TO_SYNTAX(syntax)->quoted, other);
  }
  return equals(syntax, other);
}

int syntax_exact(const Value syntax, const void *other) {
  if (syntax.type == VALUE_SYNTAX) {
    return TO_SYNTAX(syntax)->quoted.type & VALUE_OBJECT
      && TO_SYNTAX(syntax)->quoted.object == other;
  }
  return syntax.type & VALUE_OBJECT && syntax.object == other;
}

int syntax_is_special(const Value syntax, const Symbol *symbol, int arity) {
  if (!syntax_is(syntax, VALUE_VECTOR)) {
    return 0;
  }
  Vector *v = TO_VECTOR(syntax_get(syntax));
  if (v->length != arity + 1) {
    return 0;
  }
  return syntax_exact(v->cells[0], symbol);
}

int syntax_is_string_like(const Value syntax) {
  const Value v = syntax.type == VALUE_SYNTAX ? TO_SYNTAX(syntax)->quoted : syntax;
  return v.type == VALUE_STRING || v.type == VALUE_SYMBOL || v.type == VALUE_KEYWORD;
}

String *syntax_get_string(const Value syntax) {
  const Value v = syntax.type == VALUE_SYNTAX ? TO_SYNTAX(syntax)->quoted : syntax;
  switch (v.type) {
    case VALUE_STRING:
      return copy_object(TO_STRING(v));
    default:
      return copy_object(TO_SYMBOL(v)->name);
  }
}

Value syntax_get(const Value syntax) {
  if (syntax.type == VALUE_SYNTAX) {
    return TO_SYNTAX(syntax)->quoted;
  }
  return syntax;
}

Value syntax_get_elem(int index, const Value syntax) {
  if (syntax.type == VALUE_SYNTAX && TO_SYNTAX(syntax)->quoted.type == VALUE_VECTOR) {
    return syntax_get(TO_VECTOR(TO_SYNTAX(syntax)->quoted)->cells[index]);
  } else if (syntax.type == VALUE_VECTOR) {
    return syntax_get(TO_VECTOR(syntax)->cells[index]);
  }
  return undefined;
}

/* Hash map allocation */

static Hash hash_map_hash(HashMapEntry *entry) {
  return hash(INIT_HASH, entry->key);
}

static int hash_map_equals(HashMapEntry *a, HashMapEntry *b) {
  return equals(a->key, b->key) == EQ_EQUAL;
}

HashMap *create_hash_map(void) {
  HashMap *map = allocate_object(sizeof(HashMap));
  if (!map) {
    return NULL;
  }
  map->type = NULL;
  init_generic_hash_map(&map->map, sizeof(HashMapEntry), (HashFunc) hash_map_hash, (EqualityFunc) hash_map_equals);
  return map;
}

Value hash_map_get(HashMap *map, Value key) {
  Value result = undefined;
  HashMapEntry query = { .key = key };
  HashMapEntry entry;
  if (generic_hash_map_get(&map->map, &query, &entry)) {
    result = copy_value(entry.value);
  } else {
    raise_error(domain_error, "key not found");
  }
  delete_value(key);
  delete_value(HASH_MAP(map));
  return result;
}

Value hash_map_set(HashMap *map, Value key, Value value) {
  Value result = undefined;
  int exists;
  HashMapEntry existing;
  HashMapEntry entry = { .key = key, .value = value };
  if (generic_hash_map_set(&map->map, &entry, &exists, &existing)) {
    if (exists) {
      delete_value(existing.key);
      delete_value(existing.value);
    }
    result = unit;
  } else {
    delete_value(key);
    delete_value(value);
    raise_error(out_of_memory_error, "hash map reallocation failed");
  }
  delete_value(HASH_MAP(map));
  return result;
}

Value hash_map_unset(HashMap *map, Value key) {
  Value result = undefined;
  HashMapEntry existing;
  HashMapEntry query = { .key = key };
  if (generic_hash_map_remove(&map->map, &query, &existing)) {
    delete_value(existing.key);
    result = existing.value;
  } else {
    raise_error(domain_error, "key not found");
  }
  delete_value(key);
  delete_value(HASH_MAP(map));
  return result;
}


Hash hash(Hash h, Value value) {
  h = HASH_ADD_BYTE(value.type, h);
  switch (value.type) {
    /* Primitives */

    case VALUE_UNDEFINED:
    case VALUE_UNIT:
      return h;
    case VALUE_I64:
    case VALUE_F64:
      for (int i = 0; i < 8; i++) {
        h = HASH_ADD_BYTE(GET_BYTE(i, value.i64), h);
      }
      return h;
    case VALUE_FUNC:
      return HASH_ADD_PTR(value.func, h);

    /* Reference types */

    case VALUE_VECTOR:
    case VALUE_ARRAY: {
      const Vector *v = TO_VECTOR(value);
      for (size_t i = 0; i < v->length; i++) {
        h = hash(h, v->cells[i]);
      }
      return h;
    }
    case VALUE_VECTOR_SLICE:
    case VALUE_ARRAY_SLICE: {
      const VectorSlice *v = TO_VECTOR_SLICE(value);
      for (size_t i = 0; i < v->length; i++) {
        h = hash(h, v->cells[i]);
      }
      return h;
    }
    case VALUE_ARRAY_BUFFER: {
      const ArrayBuffer *a = TO_ARRAY_BUFFER(value);
      for (size_t i = 0; i < a->length; i++) {
        h = hash(h, a->cells[i]);
      }
      return h;
    }
    case VALUE_LIST:
      for (const List *l = TO_LIST(value); l; l = l->tail) {
        h = hash(h, l->head);
      }
      return h;
    case VALUE_STRING: {
      String *string = TO_STRING(value);
      for (size_t i = 0; i < string->length; i++) {
        h = HASH_ADD_BYTE(string->bytes[i], h);
      }
      return h;
    }
    case VALUE_WEAK_REF:
      return hash(h, TO_WEAK_REF(value)->value);
    case VALUE_SYMBOL:
    case VALUE_KEYWORD:
    case VALUE_CLOSURE:
    case VALUE_POINTER:
    case VALUE_TYPE:
    case VALUE_GEN_FUNC:
      return HASH_ADD_PTR(value.object, h);
    case VALUE_DATA: {
      Data *d = TO_DATA(value);
      h = hash(h, SYMBOL(d->tag));
      for (size_t i = 0; i < d->size; i++) {
        h = hash(h, d->fields[i]);
      }
      return h;
    }
    case VALUE_SYNTAX:
      return hash(h, TO_SYNTAX(value)->quoted);
    case VALUE_HASH_MAP: {
      HashMapEntry entry;
      HashMapIterator it = generic_hash_map_iterate(&TO_HASH_MAP(value)->map);
      while (generic_hash_map_next(&it, &entry)) {
        h = hash(h, entry.key);
        h = hash(h, entry.value);
      }
      return h;
    }
  }
  return h;
}
