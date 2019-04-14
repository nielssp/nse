/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_LANG_H
#define NSE_LANG_H

typedef struct Module Module;
typedef struct Symbol Symbol;

#define TRUE (copy_value(true_value))
#define FALSE (copy_value(false_value))

extern Module *lang_module;

extern Symbol *out_of_memory_error;

extern Symbol *true_symbol;
extern Symbol *false_symbol;
extern Symbol *if_symbol;
extern Symbol *let_symbol;
extern Symbol *match_symbol;
extern Symbol *do_symbol;
extern Symbol *fn_symbol;
extern Symbol *try_symbol;
extern Symbol *loop_symbol;
extern Symbol *for_symbol;
extern Symbol *collect_symbol;
extern Symbol *recur_symbol;
extern Symbol *continue_symbol;
extern Symbol *def_symbol;
extern Symbol *def_macro_symbol;
extern Symbol *def_type_symbol;
extern Symbol *def_read_macro_symbol;
extern Symbol *def_data_symbol;
extern Symbol *def_generic_symbol;
extern Symbol *def_method_symbol;

extern Symbol *read_char_symbol;
extern Symbol *read_string_symbol;
extern Symbol *read_symbol_symbol;
extern Symbol *read_int_symbol;
extern Symbol *read_any_symbol;
extern Symbol *read_bind_symbol;
extern Symbol *read_return_symbol;
extern Symbol *read_ignore_symbol;

extern Symbol *key_keyword;
extern Symbol *opt_keyword;
extern Symbol *rest_keyword;
extern Symbol *match_keyword;

extern Value true_value;
extern Value false_value;

void init_lang_module();

#endif
