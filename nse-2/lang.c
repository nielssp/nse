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

  key_symbol = module_extern_symbol(lang_module, "&key");
}
