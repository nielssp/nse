/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_SPECIAL_H
#define NSE_SPECIAL_H

typedef struct Value Value;
typedef struct VectorSlice VectorSlice;
typedef struct Scope Scope;

Value eval_if(VectorSlice *args, Scope *scope);
Value eval_let(VectorSlice *args, Scope *scope);
Value eval_match(VectorSlice *args, Scope *scope);
Value eval_fn(VectorSlice *args, Scope *scope);
Value eval_try(VectorSlice *args, Scope *scope);
Value eval_continue(VectorSlice *args, Scope *scope);
Value eval_recur(VectorSlice *args, Scope *scope);
Value eval_def(VectorSlice *args, Scope *scope);
Value eval_def_read_macro(VectorSlice *args, Scope *scope);
Value eval_def_type(VectorSlice *args, Scope *scope);
Value eval_def_data(VectorSlice *args, Scope *scope);
Value eval_def_macro(VectorSlice *args, Scope *scope);
Value eval_def_generic(VectorSlice *args, Scope *scope);
Value eval_def_method(VectorSlice *args, Scope *scope);
Value eval_loop(VectorSlice *args, Scope *scope);

#endif
