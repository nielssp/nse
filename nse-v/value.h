/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_VALUE_H
#define NSE_VALUE_H

/* NSE values */

#include <stdint.h>
#include <stdlib.h>
#include "hashmap.h"

typedef struct Value Value;
typedef struct Object Object;
typedef struct Slice Slice;

typedef struct Module Module;
typedef struct Scope Scope;
typedef struct Type Type;

typedef struct Vector Vector;
typedef struct VectorSlice VectorSlice;
typedef struct Vector Array;
typedef struct VectorSlice ArraySlice;
typedef struct ArrayBuffer ArrayBuffer;
typedef struct List List;
typedef struct String String;
typedef struct WeakRef WeakRef;
typedef struct Symbol Symbol;
typedef struct Data Data;
typedef struct Closure Closure;
typedef struct GenFunc GenFunc;
typedef struct Pointer Pointer;
typedef struct Syntax Syntax;
typedef struct HashMap HashMap;

typedef struct VectorBuilder VectorBuilder;
typedef struct ListBuilder ListBuilder;

enum {
  /* Object type bit */
  VALUE_OBJECT = 0x20
};

/* Types of values in NSE. */
typedef enum {
  /* Primitives = */

  /* Undefined represents an error */
  VALUE_UNDEFINED    = 0x00,
  /* No value / void */
  VALUE_UNIT         = 0x01,
  /* 64-bit signed integers */
  VALUE_I64          = 0x02,
  /* 64-bit floating point */
  VALUE_F64          = 0x03,
  /* function pointer */
  VALUE_FUNC         = 0x04,

  /* Reference types */

  /* Immutable array */
  VALUE_VECTOR       = VALUE_OBJECT | 0x01,
  /* Slice of vector */
  VALUE_VECTOR_SLICE = VALUE_OBJECT | 0x02,
  /* Mutable array */
  VALUE_ARRAY        = VALUE_OBJECT | 0x03,
  /* Slice of array */
  VALUE_ARRAY_SLICE  = VALUE_OBJECT | 0x04,
  /* Growable array buffer */
  VALUE_ARRAY_BUFFER = VALUE_OBJECT | 0x05,
  /* Immutable linked list */
  VALUE_LIST         = VALUE_OBJECT | 0x06,
  /* Byte array */
  VALUE_STRING       = VALUE_OBJECT | 0x07,
  /* Weak reference */
  VALUE_WEAK_REF     = VALUE_OBJECT | 0x09,
  /* Symbol */
  VALUE_SYMBOL       = VALUE_OBJECT | 0x0A,
  /* Keyword */
  VALUE_KEYWORD      = VALUE_OBJECT | 0x0B,
  /* Instance of user-defined data type */
  VALUE_DATA         = VALUE_OBJECT | 0x0C,
  /* Syntax wrapper with positional information */
  VALUE_SYNTAX       = VALUE_OBJECT | 0x0D,
  /* Closure */
  VALUE_CLOSURE      = VALUE_OBJECT | 0x0E,
  /* Pointer to non-NSE object */
  VALUE_POINTER      = VALUE_OBJECT | 0x0F,
  /* A type */
  VALUE_TYPE         = VALUE_OBJECT | 0x10,
  /* Generic functions */
  VALUE_GEN_FUNC     = VALUE_OBJECT | 0x11,
  /* Hash maps */
  VALUE_HASH_MAP     = VALUE_OBJECT | 0x12,
} ValueType;

/* Value structure */
struct Value {
  ValueType type;
  union {
    int64_t i64;
    double f64;
    Value (*func)(Slice, Scope *);
    Object *object;
  };
};

/* Undefined value */
extern Value undefined;

/* Unit value */
extern Value unit;

/* Create i64 value */
#define I64(i) ((Value){ .type = VALUE_I64, .i64 = (i) })

/* Create f64 value */
#define F64(f) ((Value){ .type = VALUE_F64, .f64 = (f) })

/* Create function pointer value */
#define FUNC(f) ((Value){ .type = VALUE_FUNC, .func = (f) })

/* Get name of value type */
const char *value_type_name(ValueType type);

typedef enum {
  EQ_EQUAL,
  EQ_NOT_EQUAL,
  EQ_ERROR
} Equality;

#define B_TO_EQ(b) ((b) ? EQ_EQUAL : EQ_NOT_EQUAL)

/* Compare values a and b */
Equality equals(const Value a, const Value b);

/* Object structure */
struct Object {
  size_t refs;
  WeakRef *weak_refs;
};

/* Dynamically allocate an object of the given size */
void *allocate_object(size_t size);

/* Copy a value (increment object ref-count) */
Value copy_value(Value val);

/* Copy an object */
void *copy_object(void *);

/* Delete a value (decrement object ref-count) */
void delete_value(Value val);

/* Check object allocation. Returns undefined if v is an object reference set to
 * NULL*/
Value check_alloc(Value v);

/* Evaluates to true if value is not undefined */
#define RESULT_OK(value) ((value).type != VALUE_UNDEFINED)

/* Evaluates next if previous is not undefined */
#define THEN(previous, next) ((RESULT_OK(previous)) ? (next) : undefined)

/* Evaluates next if previous is not NULL */
#define THENP(previous, next) ((previous) ? (next) : NULL)

/* Generic stack allocated slices */

struct Slice {
  Value sequence;
  size_t length;
  Value *cells;
};

#define SLICE_OK(slice) (RESULT_OK((slice).sequence))
#define SLICE_ERROR ((Slice){ .sequence = undefined, .length = 0, .cells = NULL })

/* Convert a sequence to a slice (allocation-free) */
Slice to_slice(Value sequence);

size_t get_slice_length(const Value sequence);

/* Slice a sequence (allocation-free) */
Slice slice(Value sequence, size_t offset, size_t length);

/* Slice a slice (allocation-free) */
Slice slice_slice(Slice slice, size_t offset, size_t length);

/* Convert a slice to a sequence (may allocate) */
Value slice_to_value(Slice slice);

/* Copy slice (increment reference counter of sequence) */
Slice copy_slice(Slice slice);

/* Delete slice (decrement reference counter of sequence) */
void delete_slice(Slice slice);



/* Vectors */

/* Vector object */
struct Vector {
  Object header;
  /* Number of elements */
  size_t length;
  Type *type;
  Value cells[];
};

/* Convert Vector * to Value */
#define VECTOR(v) ((Value){ .type = VALUE_VECTOR, .object = (Object *)(v) })

/* Convert Value to Vector * */
#define TO_VECTOR(val) ((Vector *)(val).object)

/* Allocate a vector of the given size */
Vector *create_vector(size_t length);

struct VectorBuilder {
  size_t size;
  Vector *vector;
};

VectorBuilder create_vector_builder();

VectorBuilder vector_builder_push(VectorBuilder builder, Value value);



/* Vector slices */

/* Vector slice object */
struct VectorSlice {
  Object header;
  size_t length;
  Type *type;
  Vector *vector;
  Value *cells;
};

/* Convert VectorSlice * to Value */
#define VECTOR_SLICE(v) ((Value){ .type = VALUE_VECTOR_SLICE, .object = (Object *)(v) })

/* Convert Value to VectorSlice * */
#define TO_VECTOR_SLICE(val) ((VectorSlice *)(val).object)

/* Allocate a vector slice */
VectorSlice *create_vector_slice(Vector *parent, size_t offset, size_t length);

/* Create a vector slice from another slice */
VectorSlice *slice_vector_slice(VectorSlice *parent, size_t offset, size_t length);



/* Arrays */

/* Convert Array * to Value */
#define ARRAY(v) ((Value){ .type = VALUE_ARRAY, .object = (Object *)(v) })

/* Convert Value to Array * */
#define TO_ARRAY(val) ((Array *)(val).object)

/* Allocate an array of the given size */
Array *create_array(size_t length);

Value array_set(Array *array, size_t index, Value value);



/* Array slices */

/* Convert ArraySlice * to Value */
#define ARRAY_SLICE(v) ((Value){ .type = VALUE_ARRAY_SLICE, .object = (Object *)(v) })

/* Convert Value to ArraySlice * */
#define TO_ARRAY_SLICE(val) ((ArraySlice *)(val).object)

/* Allocate an array slice */
ArraySlice *create_array_slice(Array *parent, size_t offset, size_t length);

/* Create an array slice from another slice */
ArraySlice *slice_array_slice(ArraySlice *parent, size_t offset, size_t length);

Value array_slice_set(ArraySlice *array_slice, size_t index, Value value);



/* Array buffers */

/* Array buffer object */
struct ArrayBuffer {
  Object header;
  size_t size;
  /* Number of elements */
  size_t length;
  Type *type;
  Value *cells;
};

/* Convert ArrayBuffer * to Value */
#define ARRAY_BUFFER(v) ((Value){ .type = VALUE_ARRAY_BUFFER, .object = (Object *)(v) })

/* Convert Value to ArrayBuffer * */
#define TO_ARRAY_BUFFER(val) ((ArrayBuffer *)(val).object)

/* Allocate an array buffer of the given size */
ArrayBuffer *create_array_buffer(size_t initial_size);

ArrayBuffer *slice_array_buffer(ArrayBuffer *buffer, size_t offset, size_t length);

Value array_buffer_set(ArrayBuffer *array_buffer, size_t index, Value value);

Value array_buffer_delete(ArrayBuffer *array_buffer, size_t index);

ArrayBuffer *array_buffer_push(ArrayBuffer *array_buffer, Value value);

Value array_buffer_pop(ArrayBuffer *array_buffer);

ArrayBuffer *array_buffer_insert(ArrayBuffer *array_buffer, size_t index, Value value);



/* Linked lists */

/* Linked list node */
struct List {
  Object header;
  Value head;
  List *tail;
};

/* Convert List * to Value */
#define LIST(v) ((Value){ .type = VALUE_LIST, .object = (Object *)(v) })

/* Convert Value to List * */
#define TO_LIST(val) ((List *)(val).object)

/* Allocate a list node */
List *create_list(Value head, List *tail);



/* Strings
 * 
 * Strings are sequences of bytes. The sequence always contains a trailing
 * NUL-byte allowing for easy conversion to C-strings. */

/* String object */
struct String {
  Object header;
  /* Length of string */
  size_t length;
  /* Array of bytes plus a trailing NUL-byte */
  uint8_t bytes[];
};

/* Convert String * to Value */
#define STRING(v) ((Value){ .type = VALUE_STRING, .object = (Object *)(v) })

/* Convert Value to String * */
#define TO_STRING(val) ((String *)(val).object)

/* Convert String * to C-string */
#define TO_C_STRING(string) ((const char *)(string)->bytes)

/* Allocate a string
 * Parameters:
 *   bytes - array of bytes
 *   length - length of string
 */
String *create_string(const uint8_t *bytes, size_t length);

/* Convert C-string to String */
String *c_string_to_string(const char *str);

/* Create empty string buffer */
String *create_string_buffer(size_t capacity);

/* Resize string buffer */
String *resize_string_buffer(String *s, size_t new_capacity);

String *slice_string(String *string, size_t offset, size_t length);



/* Weak references */

/* Weak reference object
 * When created, it is added to `value`'s internal doubly linked list of weak
 * references. When `value` is deleted, all its weak references are set to NULL.
 */
struct WeakRef {
  Object header;
  Type *type;
  /* Next weak reference in doubly linked list */
  WeakRef *next;
  /* Previous weak reference in doubly linked list */
  WeakRef *previous;
  /* Target of reference */
  Value value;
};

/* Convert WeakRef * to Value */
#define WEAK_REF(v) ((Value){ .type = VALUE_WEAK_REF, .object = (Object *)(v) })

/* Convert Value to WeakRef * */
#define TO_WEAK_REF(val) ((WeakRef *)(val).object)

/* Create a weak reference to another object */
WeakRef *create_weak_ref(Value object);


/* Symbols */

struct Symbol {
  Object header;
  /* Module in which the symbol is interned, NULL if uninterned */
  Module *module;
  String *name;
};

/* Convert Symbol * to Value */
#define SYMBOL(v) ((Value){ .type = VALUE_SYMBOL, .object = (Object *)(v) })

/* Convert Value to Symbol * */
#define TO_SYMBOL(val) ((Symbol *)(val).object)

/* Create a symbol */
Symbol *create_symbol(String *name, Module *module);



/* Data */

struct Data {
  Object header;
  Type *type;
  Symbol *tag;
  size_t size;
  Value fields[];
};

/* Convert Data * to Value */
#define DATA(v) ((Value){ .type = VALUE_DATA, .object = (Object *)(v) })

/* Convert Value to Data * */
#define TO_DATA(val) ((Data *)(val).object)

/* Create data */
Data *create_data(Type *type, Symbol *tag, Value const fields[], size_t size);



/* Closures */

/* Closure function type */
typedef Value (* ClosureFunc)(Slice, const Closure *, Scope *scope);

struct Closure {
  Object header;
  ClosureFunc f;
  size_t env_size;
  Value env[];
};

/* Convert Closure * to Value */
#define CLOSURE(v) ((Value){ .type = VALUE_CLOSURE, .object = (Object *)(v) })

/* Convert Value to Closure * */
#define TO_CLOSURE(val) ((Closure *)(val).object)

/* Create a closure */
Closure *create_closure(ClosureFunc f, Value const env[], size_t env_size);



/* Generic functions */
struct GenFunc {
  Object header;
  Symbol *name;
  Module *context;
  uint8_t min_arity;
  uint8_t variadic;
  uint8_t type_parameters;
  int8_t parameter_indices[];
};

/* Convert GenFunc * to Value */
#define GEN_FUNC(v) ((Value){ .type = VALUE_GEN_FUNC, .object = (Object *)(v) })

/* Convert Value to GenFunc * */
#define TO_GEN_FUNC(val) ((GenFunc *)(val).object)

/* Create a generic function */
GenFunc *create_gen_func(Symbol *name, Module *context, uint8_t min_arity, uint8_t variadic, uint8_t type_parameters, int8_t const parameter_indices[]);



/* Pointers */

/* Pointer destructor type */
typedef void (* Destructor)(void *);

struct Pointer {
  Object header;
  Type *type;
  void *pointer;
  Destructor destructor;
};

/* Convert Pointer * to Value */
#define POINTER(v) ((Value){ .type = VALUE_POINTER, .object = (Object *)(v) })

/* Convert Value to Pointer * */
#define TO_POINTER(val) ((Pointer *)(val).object)

/* Create a pointer */
Pointer *create_pointer(Type *type, void *pointer, Destructor destructor);

/* Empty destructor */
void void_destructor(void *p);



/* Syntax */

struct Syntax {
  Object header;
  size_t start_line;
  size_t start_column;
  size_t end_line;
  size_t end_column;
  String *file;
  Value quoted;
};

/* Convert Syntax * to Value */
#define SYNTAX(v) ((Value){ .type = VALUE_SYNTAX, .object = (Object *)(v) })

/* Convert Value to Syntax * */
#define TO_SYNTAX(val) ((Syntax *)(val).object)

/* Create syntax object */
Syntax *create_syntax(Value value);

/* Recursively remove syntax annotations */
Value syntax_to_datum(Value v);

/* Check type of quoted value */
int syntax_is(const Value syntax, ValueType type);

/* Check quoted value for equality */
Equality syntax_equals(const Value syntax, const Value other);

/* Check quoted value for object reference equality */
int syntax_exact(const Value syntax, const void *other);

int syntax_is_special(const Value syntax, const Symbol *symbol, int arity);

int syntax_is_string_like(const Value syntax);

String *syntax_get_string(const Value syntax);

/* Get quoted value */
Value syntax_get(const Value syntax);

/* Get element of quoted vector */
Value syntax_get_elem(int index, const Value syntax);



/* Hash maps */

struct HashMap {
  Object header;
  Type *type;
  GenericHashMap map;
};

typedef struct {
  Value key;
  Value value;
} HashMapEntry;

/* Convert HashMap * to Value */
#define HASH_MAP(v) ((Value){ .type = VALUE_HASH_MAP, .object = (Object *)(v) })

/* Convert Value to HashMap * */
#define TO_HASH_MAP(val) ((HashMap *)(val).object)

/* Create hash map object */
HashMap *create_hash_map(void);

Value hash_map_get(HashMap *map, Value key);

Value hash_map_set(HashMap *map, Value key, Value value);

Value hash_map_unset(HashMap *map, Value key);

Hash hash(Hash h, Value value);



/* Type */

/* Convert Type * to Value */
#define TYPE(v) ((Value){ .type = VALUE_TYPE, .object = (Object *)(v) })

/* Convert Value to Type * */
#define TO_TYPE(val) ((Type *)(val).object)

#endif
