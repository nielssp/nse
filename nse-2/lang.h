#ifndef LANG_H
#define LANG_H

#define TRUE (SYMBOL(t_symbol))
#define FALSE (SYMBOL(f_symbol))

extern Module *lang_module;

extern Symbol *t_symbol;
extern Symbol *f_symbol;
extern Symbol *if_symbol;
extern Symbol *let_symbol;
extern Symbol *fn_symbol;
extern Symbol *try_symbol;
extern Symbol *def_symbol;
extern Symbol *def_macro_symbol;
extern Symbol *def_type_symbol;

extern Symbol *key_symbol;

void init_lang_module();

#endif
