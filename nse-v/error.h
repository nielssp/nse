/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_ERROR_H
#define NSE_ERROR_H

#include <stdlib.h>

typedef struct Symbol Symbol;
typedef struct Module Module;
typedef struct Value Value;
typedef struct Syntax Syntax;
typedef struct List List;

extern Module *error_module;

extern Symbol *out_of_memory_error;
extern Symbol *domain_error;
extern Symbol *pattern_error;
extern Symbol *name_error;
extern Symbol *io_error;
extern Symbol *syntax_error;

extern Syntax *error_form;

void init_error_module();
void raise_error(Symbol *error_type, const char *format, ...);
const char *current_error();
Symbol *current_error_type();
void clear_error();
void *allocate(size_t bytes);

void set_debug_form(Value form);
Syntax *push_debug_form(Syntax *syntax);
Value pop_debug_form(Value result, Syntax *previous);

int stack_trace_push(Value func, Value args);
void stack_trace_pop();
List *get_stack_trace();
void clear_stack_trace();

#endif
