#include "runtime/value.h"
#include "runtime/error.h"

#include "lang.h"

Symbol *true_symbol = NULL;
Symbol *false_symbol = NULL;
Symbol *if_symbol = NULL;
Symbol *let_symbol = NULL;
Symbol *match_symbol = NULL;
Symbol *fn_symbol = NULL;
Symbol *try_symbol = NULL;
Symbol *loop_symbol = NULL;
Symbol *continue_symbol = NULL;
Symbol *def_symbol = NULL;
Symbol *def_macro_symbol = NULL;
Symbol *def_type_symbol = NULL;
Symbol *def_read_macro_symbol = NULL;
Symbol *def_data_symbol = NULL;

Symbol *read_char_symbol = NULL;
Symbol *read_string_symbol = NULL;
Symbol *read_symbol_symbol = NULL;
Symbol *read_int_symbol = NULL;
Symbol *read_any_symbol = NULL;
Symbol *read_bind_symbol = NULL;
Symbol *read_return_symbol = NULL;
Symbol *read_ignore_symbol = NULL;

Symbol *key_keyword = NULL;
Symbol *opt_keyword = NULL;
Symbol *rest_keyword = NULL;
Symbol *match_keyword = NULL;

NseVal true_value;
NseVal false_value;

Module *lang_module = NULL;

void init_lang_module() {
  init_values();
  init_error_module();

  lang_module = create_module("lang");

  true_symbol = module_extern_symbol(lang_module, "true"); 
  false_symbol = module_extern_symbol(lang_module, "false");
  if_symbol = module_extern_symbol(lang_module, "if");
  let_symbol = module_extern_symbol(lang_module, "let");
  match_symbol = module_extern_symbol(lang_module, "match");
  fn_symbol = module_extern_symbol(lang_module, "fn");
  try_symbol = module_extern_symbol(lang_module, "try");
  loop_symbol = module_extern_symbol(lang_module, "loop");
  continue_symbol = module_extern_symbol(lang_module, "continue");
  def_symbol = module_extern_symbol(lang_module, "def");
  def_macro_symbol = module_extern_symbol(lang_module, "def-macro");
  def_type_symbol = module_extern_symbol(lang_module, "def-type");
  def_read_macro_symbol = module_extern_symbol(lang_module, "def-read-macro");
  def_data_symbol = module_extern_symbol(lang_module, "def-data");

  read_char_symbol = module_extern_symbol(lang_module, "read-char");
  read_string_symbol = module_extern_symbol(lang_module, "read-string");
  read_symbol_symbol = module_extern_symbol(lang_module, "read-symbol");
  read_int_symbol = module_extern_symbol(lang_module, "read-int");
  read_any_symbol = module_extern_symbol(lang_module, "read-any");
  read_bind_symbol = module_extern_symbol(lang_module, "read-bind");
  read_return_symbol = module_extern_symbol(lang_module, "read-return");
  read_ignore_symbol = module_extern_symbol(lang_module, "read-ignore");

  key_keyword = module_extern_symbol(lang_module, "&key");
  opt_keyword = module_extern_symbol(lang_module, "&opt");
  rest_keyword = module_extern_symbol(lang_module, "&rest");
  match_keyword = module_extern_symbol(lang_module, "&match");

  true_value = DATA(create_data(copy_type(bool_type), true_symbol, NULL, 0));
  module_define(lang_module, "true", true_value);
  false_value = DATA(create_data(copy_type(bool_type), false_symbol, NULL, 0));
  module_define(lang_module, "false", false_value);
}
