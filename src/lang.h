#ifndef LANG_H
#define LANG_H

#define TRUE (add_ref(true_value))
#define FALSE (add_ref(false_value))

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

extern NseVal true_value;
extern NseVal false_value;

void init_lang_module();

#endif
