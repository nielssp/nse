#include "nsert.h"

Symbol *t_symbol = NULL;
Symbol *f_symbol = NULL;
Symbol *if_symbol = NULL;
Symbol *let_symbol = NULL;
Symbol *fn_symbol = NULL;
Symbol *try_symbol = NULL;
Symbol *def_symbol = NULL;
Symbol *def_macro_symbol = NULL;
Symbol *def_type_symbol = NULL;
Symbol *def_read_macro_symbol = NULL;

Symbol *scope_symbol = NULL;

Symbol *read_char_symbol = NULL;
Symbol *read_string_symbol = NULL;
Symbol *read_symbol_symbol = NULL;
Symbol *read_int_symbol = NULL;
Symbol *read_list_symbol = NULL;
Symbol *read_any_symbol = NULL;
Symbol *read_bind_symbol = NULL;
Symbol *read_return_symbol = NULL;

Symbol *key_symbol = NULL;

Module *lang_module = NULL;

void init_lang_module() {
  init_error_module();

  lang_module = create_module("lang");

  t_symbol = module_extern_symbol(lang_module, "t");
  f_symbol = module_extern_symbol(lang_module, "f");
  if_symbol = module_extern_symbol(lang_module, "if");
  let_symbol = module_extern_symbol(lang_module, "let");
  fn_symbol = module_extern_symbol(lang_module, "fn");
  try_symbol = module_extern_symbol(lang_module, "try");
  def_symbol = module_extern_symbol(lang_module, "def");
  def_macro_symbol = module_extern_symbol(lang_module, "def-macro");
  def_type_symbol = module_extern_symbol(lang_module, "def-type");
  def_read_macro_symbol = module_extern_symbol(lang_module, "def-read-macro");

  scope_symbol = module_extern_symbol(lang_module, "scope");

  read_char_symbol = module_extern_symbol(lang_module, "read-char");
  read_string_symbol = module_extern_symbol(lang_module, "read-string");
  read_symbol_symbol = module_extern_symbol(lang_module, "read-symbol");
  read_int_symbol = module_extern_symbol(lang_module, "read-int");
  read_list_symbol = module_extern_symbol(lang_module, "read-list");
  read_any_symbol = module_extern_symbol(lang_module, "read-any");
  read_bind_symbol = module_extern_symbol(lang_module, "read-bind");
  read_return_symbol = module_extern_symbol(lang_module, "read-return");

  key_symbol = module_extern_symbol(lang_module, "&key");
}
