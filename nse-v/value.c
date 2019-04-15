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
    case VALUE_LIST: return "list";
    case VALUE_STRING: return "string";
    case VALUE_QUOTE: return "quote";
    case VALUE_TYPE_QUOTE: return "type-quote";
    case VALUE_WEAK_REF: return "weak-ref";
    case VALUE_SYMBOL: return "symbol";
    case VALUE_KEYWORD: return "keyword";
    case VALUE_DATA: return "data";
    case VALUE_SYNTAX: return "syntax";
    case VALUE_CLOSURE: return "closure";
    case VALUE_POINTER: return "pointer";
    case VALUE_TYPE: return "type";
    default: return "???";
  }
}

static Equality cells_equal(Value *a, size_t a_length, Value *b, size_t b_length) {
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

Equality equals(Value a, Value b) {
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


    case VALUE_VECTOR: {
      Vector *va = TO_VECTOR(a);
      Vector *vb = TO_VECTOR(b);
      return cells_equal(va->cells, va->length, vb->cells, vb->length);
    }
    case VALUE_VECTOR_SLICE: {
      VectorSlice *va = TO_VECTOR_SLICE(a);
      VectorSlice *vb = TO_VECTOR_SLICE(b);
      return cells_equal(va->cells, va->length, vb->cells, vb->length);
    }
    case VALUE_ARRAY:
      return EQ_ERROR;
    case VALUE_ARRAY_SLICE:
      return EQ_ERROR;
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
    case VALUE_QUOTE:
    case VALUE_TYPE_QUOTE:
      return equals(TO_QUOTE(a)->quoted, TO_QUOTE(b)->quoted);
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
      case VALUE_LIST:
        delete_value(TO_LIST(val)->head);
        if (TO_LIST(val)->tail) {
          delete_value(LIST(TO_LIST(val)->tail));
        }
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
    delete_value(VECTOR(parent));
    return NULL;
  }
  vector_slice->length = length;
  vector_slice->vector = parent;
  vector_slice->cells = parent->cells + offset;;
  return vector_slice;
}

VectorSlice *slice_vector_slice(VectorSlice *parent, size_t offset, size_t length) {
  VectorSlice *vector_slice = allocate_object(sizeof(VectorSlice));
  if (vector_slice) {
    vector_slice->length = length;
    vector_slice->vector = copy_object(parent->vector);
    vector_slice->cells = parent->cells + offset;;
  }
  delete_value(VECTOR_SLICE(parent));
  return vector_slice;
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
    case VALUE_QUOTE: {
      Value quoted = syntax_to_datum(copy_value(TO_QUOTE(v)->quoted));
      delete_value(v);
      if (RESULT_OK(quoted)) {
        return check_alloc(QUOTE(create_quote(quoted)));
      }
      delete_value(quoted);
      return undefined;
    }
    default:
      return v;
  }
}

int syntax_is(Value syntax, ValueType type) {
  return syntax.type == type || (syntax.type == VALUE_SYNTAX && TO_SYNTAX(syntax)->quoted.type == type);
}

Equality syntax_equals(Value syntax, Value other) {
  if (syntax.type == VALUE_SYNTAX) {
    return equals(TO_SYNTAX(syntax)->quoted, other);
  }
  return equals(syntax, other);
}

Value syntax_get(Value syntax) {
  if (syntax.type == VALUE_SYNTAX) {
    return TO_SYNTAX(syntax)->quoted;
  }
  return syntax;
}
