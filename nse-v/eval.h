/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */

typedef struct Value Value;
typedef struct Scope Scope;
typedef struct Slice Slice;

Value apply(Value function, Slice args);

Value eval_block(Slice block, Scope *scope);

Value eval(Value code, Scope *scope);
