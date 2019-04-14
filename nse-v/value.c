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
      case VALUE_VECTOR: {
        Vector *vector = TO_VECTOR(val);
        for (int i = 0; i < vector->length; i++) {
          delete_value(vector->cells[i]);
        }
        break;
      }
      case VALUE_VECTOR_SLICE:
        delete_value(VECTOR(TO_VECTOR_SLICE(val)->vector));
        break;
      case VALUE_STRING:
        break;
      case VALUE_QUOTE:
      case VALUE_TYPE_QUOTE:
        delete_value(TO_QUOTE(val)->quoted);
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
      case VALUE_TYPE:
        delete_type(TO_TYPE(val));
        break;
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

/* Vector slice allocation */

VectorSlice *create_vector_slice(Vector *parent, size_t offset, size_t length) {
  VectorSlice *vector_slice = allocate_object(sizeof(VectorSlice));
  if (!vector_slice) {
    return NULL;
  }
  vector_slice->length = length;
  vector_slice->vector = parent;
  vector_slice->cells = parent->cells + offset;;
  return vector_slice;
}

/* String allocation */

String *create_string(const uint8_t *bytes, size_t length) {
  String *string = allocate_object(sizeof(String) + length + 1);
  if (!string) {
    return NULL;
  }
  string->length = 1;
  memcpy(string->bytes, bytes, length);
  string->bytes[length] = 0;
  return string;
}

String *c_string_to_string(const char *str) {
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

/* Quote allocation */

Quote *create_quote(Value quoted) {
  Quote *quote = allocate_object(sizeof(Quote));
  if (!quote) {
    delete_value(quoted);
    return NULL;
  }
  quote->quoted = quoted;
  return quote;
}

/* Weak reference allocation */

WeakRef *create_weak_ref(Value object) {
  WeakRef *ref = allocate_object(sizeof(WeakRef));
  if (!ref) {
    delete_value(object);
    return NULL;
  }
  ref->value = object;
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

Data *create_data(Type *type, Symbol *tag, Value fields[], size_t size) {
  Data *data = allocate_object(sizeof(Data) + size * sizeof(Value));
  if (!data) {
    delete_type(type);
    delete_value(SYMBOL(tag));
    return NULL;
  }
  data->type =type;
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

Closure *create_closure(ClosureFunc f, Value env[], size_t env_size);

/* Pointer allocation */

Pointer *create_pointer(Type *type, void *pointer, Destructor destructor);

void void_destructor(void *p);

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

