/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "type.h"
#include "error.h"
#include "module.h"

#include "lang.h"

Symbol *quote_symbol = NULL;
Symbol *type_symbol = NULL;
Symbol *backquote_symbol = NULL;
Symbol *unquote_symbol = NULL;
Symbol *splice_symbol = NULL;

Symbol *true_symbol = NULL;
Symbol *false_symbol = NULL;
Symbol *if_symbol = NULL;
Symbol *let_symbol = NULL;
Symbol *match_symbol = NULL;
Symbol *do_symbol = NULL;
Symbol *fn_symbol = NULL;
Symbol *try_symbol = NULL;
Symbol *loop_symbol = NULL;
Symbol *for_symbol = NULL;
Symbol *collect_symbol = NULL;
Symbol *recur_symbol = NULL;
Symbol *continue_symbol = NULL;
Symbol *def_symbol = NULL;
Symbol *def_macro_symbol = NULL;
Symbol *def_type_symbol = NULL;
Symbol *def_read_macro_symbol = NULL;
Symbol *def_data_symbol = NULL;
Symbol *def_generic_symbol = NULL;
Symbol *def_method_symbol = NULL;

Symbol *read_char_symbol = NULL;
Symbol *read_string_symbol = NULL;
Symbol *read_symbol_symbol = NULL;
Symbol *read_int_symbol = NULL;
Symbol *read_any_symbol = NULL;
Symbol *read_bind_symbol = NULL;
Symbol *read_return_symbol = NULL;
Symbol *read_ignore_symbol = NULL;

Symbol *ellipsis_symbol = NULL;

Symbol *key_keyword = NULL;
Symbol *opt_keyword = NULL;
Symbol *rest_keyword = NULL;
Symbol *match_keyword = NULL;

Symbol *ok_symbol = NULL;
Symbol *error_symbol = NULL;

Value true_value;
Value false_value;

Module *lang_module = NULL;

void init_lang_module(void) {
  init_types();
  init_error_module();

  lang_module = create_module("lang");

  quote_symbol = module_extern_symbol_c(lang_module, "quote");
  type_symbol = module_extern_symbol_c(lang_module, "type");
  backquote_symbol = module_extern_symbol_c(lang_module, "backquote");
  unquote_symbol = module_extern_symbol_c(lang_module, "unquote");
  splice_symbol = module_extern_symbol_c(lang_module, "splice");

  true_symbol = module_extern_symbol_c(lang_module, "true"); 
  false_symbol = module_extern_symbol_c(lang_module, "false");
  if_symbol = module_extern_symbol_c(lang_module, "if");
  let_symbol = module_extern_symbol_c(lang_module, "let");
  match_symbol = module_extern_symbol_c(lang_module, "match");
  do_symbol = module_extern_symbol_c(lang_module, "do");
  fn_symbol = module_extern_symbol_c(lang_module, "fn");
  try_symbol = module_extern_symbol_c(lang_module, "try");
  loop_symbol = module_extern_symbol_c(lang_module, "loop");
  for_symbol = module_extern_symbol_c(lang_module, "for");
  collect_symbol = module_extern_symbol_c(lang_module, "collect");
  recur_symbol = module_extern_symbol_c(lang_module, "recur");
  continue_symbol = module_extern_symbol_c(lang_module, "continue");
  def_symbol = module_extern_symbol_c(lang_module, "def");
  def_macro_symbol = module_extern_symbol_c(lang_module, "def-macro");
  def_type_symbol = module_extern_symbol_c(lang_module, "def-type");
  def_read_macro_symbol = module_extern_symbol_c(lang_module, "def-read-macro");
  def_data_symbol = module_extern_symbol_c(lang_module, "def-data");
  def_generic_symbol = module_extern_symbol_c(lang_module, "def-generic");
  def_method_symbol = module_extern_symbol_c(lang_module, "def-method");

  read_char_symbol = module_extern_symbol_c(lang_module, "read-char");
  read_string_symbol = module_extern_symbol_c(lang_module, "read-string");
  read_symbol_symbol = module_extern_symbol_c(lang_module, "read-symbol");
  read_int_symbol = module_extern_symbol_c(lang_module, "read-int");
  read_any_symbol = module_extern_symbol_c(lang_module, "read-any");
  read_bind_symbol = module_extern_symbol_c(lang_module, "read-bind");
  read_return_symbol = module_extern_symbol_c(lang_module, "read-return");
  read_ignore_symbol = module_extern_symbol_c(lang_module, "read-ignore");

  ellipsis_symbol = module_extern_symbol_c(lang_module, "...");

  key_keyword = module_extern_symbol_c(lang_module, "&key");
  opt_keyword = module_extern_symbol_c(lang_module, "&opt");
  rest_keyword = module_extern_symbol_c(lang_module, "&rest");
  match_keyword = module_extern_symbol_c(lang_module, "&match");

  ok_symbol = module_extern_symbol_c(lang_module, "ok");
  error_symbol = module_extern_symbol_c(lang_module, "error");

  true_value = DATA(create_data(copy_type(bool_type), copy_object(true_symbol), NULL, 0));
  module_define(copy_object(true_symbol), true_value);
  false_value = DATA(create_data(copy_type(bool_type), copy_object(false_symbol), NULL, 0));
  module_define(copy_object(false_symbol), false_value);
}

int is_true(const Value value) {
  return value.object == true_value.object;
}
