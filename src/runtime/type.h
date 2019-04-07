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
 *     void f(CType *t) {
 *       // f owns t and must delete it after use
 *       delete_type(t);
 *     }
 *     void g(const CType *t) {
 *       // g does not own t
 *       // t may not be moved or deleted
 *     }
 *     void h(CType *t) {
 *       g(t); // OK, const
 *       f(copy_type(t)); // OK, copy to f
 *       f(t); // OK, move to f
 *       // t may no longer be used in h
 *     }
 */

typedef struct Symbol Symbol;
typedef struct PolyParam PolyParam;
typedef struct CType CType;
typedef struct GType GType;
typedef struct CTypeArray CTypeArray;

/* Internal value types. */
typedef enum {
  INTERNAL_NOTHING,
  INTERNAL_NIL,
  INTERNAL_CONS,
  INTERNAL_LIST_BUILDER,
  INTERNAL_I64,
  INTERNAL_F64,
  INTERNAL_FUNC,
  INTERNAL_CLOSURE,
  INTERNAL_GFUNC,
  INTERNAL_STRING,
  INTERNAL_SYNTAX,
  INTERNAL_SYMBOL,
  INTERNAL_REFERENCE,
  INTERNAL_TYPE,
  INTERNAL_QUOTE,
  INTERNAL_DATA,
} InternalType;

/* Types of types. */
typedef enum {
  C_TYPE_SIMPLE,
  C_TYPE_FUNC,
  C_TYPE_CLOSURE,
  C_TYPE_GFUNC,
  C_TYPE_INSTANCE,
  C_TYPE_POLY_INSTANCE,
  C_TYPE_POLY_VAR,
} CTypeType;

/* Concrete type structure. */
struct CType {
  /* Number of references. */
  size_t refs;
  /* Type of type. */
  CTypeType type;
  /* Internal type. */
  InternalType internal;
  /* Optional supertype. */
  CType *super;
  /* Optional name. */
  Symbol *name;
  union {
    /* C_TYPE_FUNC / C_TYPE_CLOSURE | C_TYPE_GFUNC */
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
      CTypeArray *parameters;
    } instance;
    /* C_TYPE_POLY_INSTANCE */
    GType *poly_instance;
    /* C_TYPE_POLY_VAR */
    struct {
      /* Generic type. */
      GType *type;
      /* Parameter index. */
      int index;
    } poly_var;
  };
};

struct CTypeArray {
  size_t refs;
  size_t size;
  CType *elements[];
};

/* any */
extern CType *any_type;
/* bool < any */
extern CType *bool_type;
/* improper-list < any */
extern CType *improper_list_type;
/* proper-list < improper-list < any */
extern CType *proper_list_type;
/* nil < (forall (t) (list t)) < proper-list < improper-list < any */
extern CType *nil_type;
/* list-builder < any */
extern CType *list_builder_type;
/* num < any */
extern CType *num_type;
/* int < num < any */
extern CType *int_type;
/* float < num < any */
extern CType *float_type;
/* i64 < int < num < any */
extern CType *i64_type;
/* f64 < float < num < any */
extern CType *f64_type;
/* string < any */
extern CType *string_type;
/* symbol < any */
extern CType *symbol_type;
/* keyword < any */
extern CType *keyword_type;
/* quote < any */
extern CType *quote_type;
/* continue < any */
extern CType *continue_type;
/* type-quote < any */
extern CType *type_quote_type;
/* type < any */
extern CType *type_type;
/* syntax < any */
extern CType *syntax_type;
/* func < any */
extern CType *func_type;
/* scope < any */
extern CType *scope_type;
/* stream < any */
extern CType *stream_type;
/* generic-type < any */
extern CType *generic_type_type;

/* Generic list type. */
extern GType *list_type;

/* Initializes all built-in types. */
void init_types();

/* Creates a simple type. May raise an error and return NULL if allocation
 * fails.
 * Parameters:
 *   internal - The internal representation of the type.
 *   super - The supertype, not implicitly copied. Deleted on error.
 */
CType *create_simple_type(InternalType internal, CType *super);
/* Moves the type (does not touch reference counter). */
#define move_type(t) (t)
/* Copies the type (by incrementing its reference counter). */
CType *copy_type(CType *t);
/* Deletes the type (by decrementing its reference counter). */
void delete_type(CType *t);

/* Create a type array. May raise an error and return NULL if allocation fails.
 * Parameters:
 *   size - Size of array.
 *   elements - Elements of array.
 */
CTypeArray *create_type_array(size_t size, CType * const elements[]);

/* Create a type array with all elements initialized to NULL. May raise an
 * error and return NULL if allocation fails.
 */
CTypeArray *create_type_array_null(size_t size);
/* Moves the type array (does not touch reference counter). */
#define move_type_array(a) (a)
/* Copies the type array (by incrementing its reference counter). */
CTypeArray *copy_type_array(CTypeArray *a);
/* Deletes the type array (by decrementing its reference counter). */
void delete_type_array(CTypeArray *a);

/* Creates a generic type. May raise an error and return NULL if allocation
 * fails.
 * Parameters:
 *   arity - Number of parameters.
 *   internal - The internal representation of instances of this type.
 *   super - The supertype, not implicitly copied. Deleted on error.
 */
GType *create_generic(int arity, InternalType internal, CType *super);
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
CType *create_poly_var(GType *g, int index);

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
CType *get_instance(GType *g, CTypeArray *parameters);
/* A shorthand for `get_instance()` for unary generic types.
 * Parameters:
 *   g - The generic type. Must have an arity of 1.
 *   parameter - The single type parameter, not implicitly copied.
 */
CType *get_unary_instance(GType *g, CType *parameter);
/* Returns a copy of the singleton polymorphic instance of a generic type.
 * May raise an error and return NULL if allocation fails. */
CType *get_poly_instance(GType *g);
/* Copies or creates a function type instance with the given arity. May raise an
 * error and return NULL if allocation fails. Function types are singletons,
 * i.e. `get_func_type(a, v) == get_func_type(a, v)`.
 * Parameters:
 *   min_arity - Minimum arity of the function.
 *   variadic - (bool) True if variadic, false otherwise.
 */
CType *get_func_type(int min_arity, int variadic);
/* Same as `get_func_type()` but for closures. All closure types are subtypes of
 * the function types with the same arity and variadicity, i.e.
 * `is_subtype_of(get_closure_type(a, v), get_func_type(a, v))`.
 */
CType *get_closure_type(int min_arity, int variadic);
/* Same as `get_func_type()` but for generic functions. All generic function types
 * are subtypes of the function types with the same arity and variadicity, i.e.
 * `is_subtype_of(get_generic_func_type(a, v), get_func_type(a, v))`.
 */
CType *get_generic_func_type(int min_arity, int variadic);

/* Replace type var occurrences in `t` with corresponding type in `parameters`.
 * Parameters:
 *   t - The type.
 *   g - The generic type.
 *   parameters - The generic type parameters.
 */
CType *instantiate_type(CType *t, const GType *g, const CTypeArray *parameters);

/* Returns a copy of the supertype of a type if it has one, otherwise NULL. */
CType *get_super_type(const CType *t);
/* Returns 1 if `a` is a subtype of or equal to `b`, 0 otherwise. */
int is_subtype_of(const CType *a, const CType *b);
/* Returns a common supertype for types `a` and `b`.
 * `any_type` is returned if no such common supertype can be found. */
const CType *unify_types(const CType *a, const CType *b);

#endif


