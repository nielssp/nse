/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */

typedef struct Value Value;
typedef struct Scope Scope;
typedef struct VectorSlice VectorSlice;

Value eval_block(VectorSlice *block, Scope *scope);

Value eval(Value code, Scope *scope);
