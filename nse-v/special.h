/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_SPECIAL_H
#define NSE_SPECIAL_H

typedef struct Slice Slice;
typedef struct Scope Scope;

Value eval_quote(Slice args, Scope *scope);
Value eval_type(Slice args, Scope *scope);
Value eval_backquote(Slice args, Scope *scope);
Value eval_if(Slice args, Scope *scope);
Value eval_let(Slice args, Scope *scope);
Value eval_match(Slice args, Scope *scope);
Value eval_fn(Slice args, Scope *scope);
Value eval_try(Slice args, Scope *scope);
Value eval_continue(Slice args, Scope *scope);
Value eval_recur(Slice args, Scope *scope);
Value eval_def(Slice args, Scope *scope);
Value eval_def_read_macro(Slice args, Scope *scope);
Value eval_def_type(Slice args, Scope *scope);
Value eval_def_data(Slice args, Scope *scope);
Value eval_def_macro(Slice args, Scope *scope);
Value eval_def_generic(Slice args, Scope *scope);
Value eval_def_method(Slice args, Scope *scope);
Value eval_loop(Slice args, Scope *scope);

#endif
