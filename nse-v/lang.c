/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "type.h"
#include "error.h"
#include "module.h"

#include "lang.h"

Symbol *macros_namespace = NULL;
Symbol *types_namespace = NULL;
Symbol *read_macros_namespace = NULL;
Symbol *eval_namespace = NULL;

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

static Value get_result_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_TYPE && args.cells[1].type == VALUE_TYPE) {
    TypeArray *a = create_type_array(2, (Type *[]){ TO_TYPE(args.cells[0]), TO_TYPE(args.cells[1]) });
    if (a) {
      result = TYPE(get_instance(copy_generic(result_type), a));
    }
  } else {
    raise_error(domain_error, "expected (result TYPE TYPE)");
  }
  delete_slice(args);
  return result;
}

static Value get_vector_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_TYPE) {
    result = TYPE(get_unary_instance(copy_generic(vector_type), copy_type(TO_TYPE(args.cells[0]))));
  } else {
    raise_error(domain_error, "expected (vector TYPE)");
  }
  delete_slice(args);
  return result;
}

static Value get_vector_slice_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_TYPE) {
    result = TYPE(get_unary_instance(copy_generic(vector_slice_type), copy_type(TO_TYPE(args.cells[0]))));
  } else {
    raise_error(domain_error, "expected (vector-slice TYPE)");
  }
  delete_slice(args);
  return result;
}

static Value get_array_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_TYPE) {
    result = TYPE(get_unary_instance(copy_generic(array_type), copy_type(TO_TYPE(args.cells[0]))));
  } else {
    raise_error(domain_error, "expected (array TYPE)");
  }
  delete_slice(args);
  return result;
}

static Value get_array_slice_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_TYPE) {
    result = TYPE(get_unary_instance(copy_generic(array_slice_type), copy_type(TO_TYPE(args.cells[0]))));
  } else {
    raise_error(domain_error, "expected (array-slice TYPE)");
  }
  delete_slice(args);
  return result;
}

static Value get_array_buffer_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_TYPE) {
    result = TYPE(get_unary_instance(copy_generic(array_buffer_type), copy_type(TO_TYPE(args.cells[0]))));
  } else {
    raise_error(domain_error, "expected (array-buffer TYPE)");
  }
  delete_slice(args);
  return result;
}

static Value get_list_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_TYPE) {
    result = TYPE(get_unary_instance(copy_generic(list_type), copy_type(TO_TYPE(args.cells[0]))));
  } else {
    raise_error(domain_error, "expected (list TYPE)");
  }
  delete_slice(args);
  return result;
}

void init_lang_module(void) {
  init_types();
  init_error_module();

  lang_module = create_module("lang");

  macros_namespace = module_extern_symbol_c(lang_module, "*macros*");
  types_namespace = module_extern_symbol_c(lang_module, "*types*");
  read_macros_namespace = module_extern_symbol_c(lang_module, "*read-macros*");
  eval_namespace = module_extern_symbol_c(lang_module, "*eval*");

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

  module_ext_define_type(lang_module, "nothing", TYPE(copy_type(nothing_type)));
  module_ext_define_type(lang_module, "any", TYPE(copy_type(any_type)));
  module_ext_define_type(lang_module, "unit", TYPE(copy_type(unit_type)));
  module_ext_define_type(lang_module, "bool", TYPE(copy_type(bool_type)));
  module_ext_define_type(lang_module, "num", TYPE(copy_type(num_type)));
  module_ext_define_type(lang_module, "int", TYPE(copy_type(int_type)));
  module_ext_define_type(lang_module, "float", TYPE(copy_type(float_type)));
  module_ext_define_type(lang_module, "i64", TYPE(copy_type(i64_type)));
  module_ext_define_type(lang_module, "f64", TYPE(copy_type(f64_type)));
  module_ext_define_type(lang_module, "string", TYPE(copy_type(string_type)));
  module_ext_define_type(lang_module, "symbol", TYPE(copy_type(symbol_type)));
  module_ext_define_type(lang_module, "keyword", TYPE(copy_type(keyword_type)));
  module_ext_define_type(lang_module, "type", TYPE(copy_type(type_type)));
  module_ext_define_type(lang_module, "syntax", TYPE(copy_type(syntax_type)));
  module_ext_define_type(lang_module, "func", TYPE(copy_type(func_type)));
  module_ext_define_type(lang_module, "scope", TYPE(copy_type(scope_type)));
  module_ext_define_type(lang_module, "stream", TYPE(copy_type(stream_type)));
  module_ext_define_type(lang_module, "generic-type", TYPE(copy_type(generic_type_type)));

  set_generic_type_name(result_type, module_ext_define_type(lang_module, "result", FUNC(get_result_type)));
  set_generic_type_name(vector_type, module_ext_define_type(lang_module, "vector", FUNC(get_vector_type)));
  set_generic_type_name(vector_slice_type, module_ext_define_type(lang_module, "vector-slice", FUNC(get_vector_slice_type)));
  set_generic_type_name(array_type, module_ext_define_type(lang_module, "array", FUNC(get_array_type)));
  set_generic_type_name(array_slice_type, module_ext_define_type(lang_module, "array-slice", FUNC(get_array_slice_type)));
  set_generic_type_name(array_buffer_type, module_ext_define_type(lang_module, "array-buffer", FUNC(get_array_buffer_type)));
  set_generic_type_name(list_type, module_ext_define_type(lang_module, "list", FUNC(get_list_type)));
}

int is_true(const Value value) {
  return value.object == true_value.object;
}
