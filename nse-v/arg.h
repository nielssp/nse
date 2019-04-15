/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_ARG_H
#define NSE_ARG_H

typedef struct Type Type;
typedef struct Scope Scope;
typedef struct Value Value;
typedef struct VectorSlice VectorSlice;

int assign_parameters(Scope **scope, VectorSlice *formal, VectorSlice *actual);
int match_pattern(Scope **scope, Value pattern, Value actual);
Type *parameters_to_type(VectorSlice *formal);

#endif
