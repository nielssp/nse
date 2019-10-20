/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_TYPE_H
#define NSE_TYPE_H

/* NSE type system.
 *
 * # Reference counting of concrete and generic types
 * 
 * Reference counts are incremented using `copy_type()` and `copy_generic()` and
 * decremented using `delete_type()` and `delete_generic()`.
 *
 * When a function returns a type, the caller must either delete the type or
 * return it.
 * When a function accepts non-const type parameters, the caller must decide to
 * either copy or move the parameters to the callee. The callee is responsible
 * for the deletion or move of its parameters. A moved type may no longer be
 * used by the caller.
 *
 * Examples:
 *
 *     void f(Type *t) {
 *       // f owns t and must delete it after use
 *       delete_type(t);
 *     }
 *     void g(const Type *t) {
 *       // g does not own t
 *       // t may not be moved or deleted
 *     }
 *     void h(Type *t) {
 *       g(t); // OK, const
 *       f(copy_type(t)); // OK, copy to f
 *       f(t); // OK, move to f
 *       // t may no longer be used in h
 *     }
 */

#include <stdlib.h>
#include "value.h"

typedef struct PolyParam PolyParam;
typedef struct Type Type;
typedef struct GType GType;
typedef struct TypeArray TypeArray;

/* Types of types. */
typedef enum {
  TYPE_SIMPLE,
  TYPE_FUNC,
  TYPE_INSTANCE,
  TYPE_POLY_INSTANCE,
  TYPE_POLY_VAR,
} TypeType;

/* Concrete type structure. */
struct Type {
  Object header;
  /* Type of type. */
  TypeType type;
  /* Optional supertype. */
  Type *super;
  /* Optional name. */
  Symbol *name;
  union {
    /* TYPE_FUNC */
    struct {
      /* Minimum arity. */
      int min_arity;
      /* Variadic = 1, Not variadoc = 0. */
      int variadic;
    } func;
    struct {
      /* Generic type. */
      GType *type;
      /* NULL terminated array of type parameters. */
      TypeArray *parameters;
    } instance;
    /* TYPE_POLY_INSTANCE */
    GType *poly_instance;
    /* TYPE_POLY_VAR */
    struct {
      /* Generic type. */
      GType *type;
      /* Parameter index. */
      int index;
    } poly_var;
  };
};

struct TypeArray {
  size_t refs;
  size_t size;
  Type *elements[];
};

/* nothing */
extern Type *nothing_type;
/* any */
extern Type *any_type;
/* unit < any */
extern Type *unit_type;
/* bool < any */
extern Type *bool_type;
/* num < any */
extern Type *num_type;
/* int < num < any */
extern Type *int_type;
/* float < num < any */
extern Type *float_type;
/* i64 < int < num < any */
extern Type *i64_type;
/* f64 < float < num < any */
extern Type *f64_type;
/* string < any */
extern Type *string_type;
/* symbol < any */
extern Type *symbol_type;
/* keyword < any */
extern Type *keyword_type;
/* continue < any */
extern Type *continue_type;
/* type < any */
extern Type *type_type;
/* syntax < any */
extern Type *syntax_type;
/* func < any */
extern Type *func_type;
/* scope < any */
extern Type *scope_type;
/* stream < any */
extern Type *stream_type;
/* generic-type < any */
extern Type *generic_type_type;

/* Generic result type. */
extern GType *result_type;
/* Generic vector type. */
extern GType *vector_type;
/* Generic vector slice type. */
extern GType *vector_slice_type;
/* Generic array type. */
extern GType *array_type;
/* Generic array slice type. */
extern GType *array_slice_type;
/* Generic array buffer type. */
extern GType *array_buffer_type;
/* Generic list type. */
extern GType *list_type;
/* Generic weak reference type. */
extern GType *weak_ref_type;
/* Generic hash map type. */
extern GType *hash_map_type;
/* Generic entry type. */
extern GType *entry_type;

/* Initializes all built-in types. */
void init_types(void);

/* Creates a simple type. May raise an error and return NULL if allocation
 * fails.
 * Parameters:
 *   super - The supertype, not implicitly copied. Deleted on error.
 */
Type *create_simple_type(Type *super);
/* Moves the type (does not touch reference counter). */
#define move_type(t) (t)
/* Copies the type (by incrementing its reference counter). */
Type *copy_type(Type *t);
/* Deletes the type (by decrementing its reference counter). */
void delete_type(Type *t);

/* Create a type array. May raise an error and return NULL if allocation fails.
 * Parameters:
 *   size - Size of array.
 *   elements - Elements of array.
 */
TypeArray *create_type_array(size_t size, Type * const elements[]);

/* Create a type array with all elements initialized to NULL. May raise an
 * error and return NULL if allocation fails.
 */
TypeArray *create_type_array_null(size_t size);
/* Moves the type array (does not touch reference counter). */
#define move_type_array(a) (a)
/* Copies the type array (by incrementing its reference counter). */
TypeArray *copy_type_array(TypeArray *a);
/* Deletes the type array (by decrementing its reference counter). */
void delete_type_array(TypeArray *a);

/* Creates a generic type. May raise an error and return NULL if allocation
 * fails.
 * Parameters:
 *   arity - Number of parameters.
 *   super - The supertype, not implicitly copied. Deleted on error.
 */
GType *create_generic(int arity, Type *super);
/* Moves the generic type (does not touch reference counter). */
#define move_generic(g) (g)
/* Copies the generic type (by incrementing its reference counter). */
GType *copy_generic(GType *g);
/* Deletes the generic type (by decrementing its reference counter). */
void delete_generic(GType *g);

/* Creates a polymorphic type variable. May raise an error and return NULL if
 * allocation fails.
 * Parameters:
 *   g - The generic type that this variable is bound to, not implicitly copied.
 *       Deleted on error.
 *   index - The index of the generic type parameter this variable is bound to.
 */
Type *create_poly_var(GType *g, int index);

/* Returns the name of a generic type if it has one. */
Symbol *generic_type_name(const GType *g);
/* Sets the name of a generic type.
 * Parameters:
 *   g - The generic type.
 *   s - The new name. Implicitely copied with add_ref(). If the generic type
 *       already has a name, the existing name is deleted.
 */
void set_generic_type_name(GType *g, Symbol *s);
/* Returns the arity of a generic type. */
int generic_type_arity(const GType *g);

/* Copies or creates an instance of a generic type. Raises an error and returns
 * NULL if the number of parameters does not match the arity, or if allocation
 * fails. Instances are singletons, so if the function is called twice with the
 * same parameters, the same object is returned (unless deleted).
 * Parameters:
 *   g - The generic type.
 *   parameters - An array of type parameters. The length of the array must be
 *                `generic_type_arity(g) + 1`.
 */
Type *get_instance(GType *g, TypeArray *parameters);
/* A shorthand for `get_instance()` for unary generic types.
 * Parameters:
 *   g - The generic type. Must have an arity of 1.
 *   parameter - The single type parameter, not implicitly copied.
 */
Type *get_unary_instance(GType *g, Type *parameter);
/* Returns a copy of the singleton polymorphic instance of a generic type.
 * May raise an error and return NULL if allocation fails. */
Type *get_poly_instance(GType *g);
/* Copies or creates a function type instance with the given arity. May raise an
 * error and return NULL if allocation fails. Function types are singletons,
 * i.e. `get_func_type(a, v) == get_func_type(a, v)`.
 * Parameters:
 *   min_arity - Minimum arity of the function.
 *   variadic - (bool) True if variadic, false otherwise.
 */
Type *get_func_type(int min_arity, int variadic);

/* Replace type var occurrences in `t` with corresponding type in `parameters`.
 * Parameters:
 *   t - The type.
 *   g - The generic type.
 *   parameters - The generic type parameters.
 */
Type *instantiate_type(Type *t, const GType *g, const TypeArray *parameters);

/* Returns a copy of the supertype of a type if it has one, otherwise NULL. */
Type *get_super_type(const Type *t);
/* Returns 1 if `a` is a subtype of or equal to `b`, 0 otherwise. */
int is_subtype_of(const Type *a, const Type *b);
/* Returns 1 if the types of a are subtypes of the types of b. */
int are_subtypes_of(const TypeArray *a, const TypeArray *b);
/* Returns a common supertype for types `a` and `b`.
 * `any_type` is returned if no such common supertype can be found. */
Type *unify_types(Type *a, Type *b);
/* Equality function for type arrays. */
int type_array_equals(const TypeArray *a, const TypeArray *b);

/* Get type of value */
Type *get_type(const Value value);

#endif


