/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "type.h"
#include "error.h"
#include "module.h"
#include "../src/util/stream.h"
#include "eval.h"

#include "system.h"

static Value sum(Slice args, Scope *dynamic_scope) {
  int64_t acc = 0;
  double facc = 0.0;
  int fp = 0;
  for (size_t i = 0; i < args.length; i++) {
    Value h = args.cells[i];
    if (h.type == VALUE_I64) {
      acc += h.i64;
    } else if (h.type == VALUE_F64) {
      facc += h.f64;
      fp = 1;
    } else {
      delete_slice(args);
      set_debug_arg_index(i);
      raise_error(domain_error, "expected number");
      return undefined;
    }
  }
  delete_slice(args);
  if (fp) {
    return F64(acc + facc);
  }
  return I64(acc);
}

static Value subtract(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    raise_error(domain_error, "too few parameters");
  } else if (args.length == 1) {
    if (args.cells[0].type == VALUE_I64) {
      result = I64(-args.cells[0].i64);
    } else if (args.cells[0].type == VALUE_F64) {
      result = F64(-args.cells[0].f64);
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "expected a number");
    }
  } else {
    int64_t acc = 0;
    double facc = 0.0;
    int fp = 0;
    for (size_t i = 0; i < args.length; i++) {
      if (args.cells[i].type == VALUE_I64) {
        if (fp) {
          facc -= args.cells[i].i64;
        } else {
          acc -= args.cells[i].i64;
        }
      } else if (args.cells[i].type == VALUE_F64) {
        if (!fp) {
          facc = acc;
          fp = 1;
        }
        facc -= args.cells[i].f64;
      } else {
        fp = -1;
        set_debug_arg_index(i);
        raise_error(domain_error, "expected a number");
        break;
      }
      if (i == 0) {
        acc = -acc;
        facc = -facc;
      }
    }
    if (fp == 1) {
      result = F64(facc);
    } else if (fp == 0) {
      result = I64(acc);
    }
  }
  delete_slice(args);
  return result;
}

static Value append(Slice args, Scope *dynamic_scope) {
  size_t length = 0;
  for (size_t i = 0; i < args.length; i++) {
    if (args.cells[i].type != VALUE_VECTOR) {
      delete_slice(args);
      set_debug_arg_index(i);
      raise_error(domain_error, "expected vector");
      return undefined;
    }
    length += TO_VECTOR(args.cells[i])->length;
  }
  Vector *result = create_vector(length);
  size_t result_i = 0;
  for (size_t i = 0; i < args.length; i++) {
    Vector *v = TO_VECTOR(args.cells[i]);
    for (size_t j = 0; j < v->length; j++) {
      result->cells[result_i++] = copy_value(v->cells[j]);
    }
  }
  delete_slice(args);
  return VECTOR(result);
}

static Value tabulate(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64) {
    Value function = args.cells[1];
    int64_t length = args.cells[0].i64;
    Vector *vector = create_vector(length);
    if (vector) {
      int ok = 1;
      for (size_t i = 0; i < length; i++) {
        Value element = apply(copy_value(function), to_slice(I64(i)), dynamic_scope);
        if (RESULT_OK(element)) {
          vector->cells[i] = element;
        } else {
          set_debug_arg_index(1);
          ok = 0;
          break;
        }
      }
      if (ok) {
        result = VECTOR(vector);
      } else {
        delete_value(VECTOR(vector));
      }
    }
  } else {
    raise_error(domain_error, "expected (tabulate INT FUNCTION)");
  }
  delete_slice(args);
  return result;
}

static Value type_of(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1) {
    result = check_alloc(TYPE(get_type(args.cells[0])));
  } else {
    raise_error(domain_error, "expected (type-of ANY)");
  }
  delete_slice(args);
  return result;
}

static Value string_length(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_STRING) {
    result = I64(TO_STRING(args.cells[0])->length);
  } else {
    raise_error(domain_error, "expected (length STRING)");
  }
  delete_slice(args);
  return result;
}

static Value vector_length(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_VECTOR) {
    result = I64(TO_VECTOR(args.cells[0])->length);
  } else {
    raise_error(domain_error, "expected (length VECTOR)");
  }
  delete_slice(args);
  return result;
}

static Value vector_slice_length(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_VECTOR_SLICE) {
    result = I64(TO_VECTOR_SLICE(args.cells[0])->length);
  } else {
    raise_error(domain_error, "expected (length VECTOR-SLICE)");
  }
  delete_slice(args);
  return result;
}


Module *get_system_module() {
  Module *system = create_module("system");
  module_ext_define(system, "+", FUNC(sum));
  module_ext_define(system, "-", FUNC(subtract));

  module_ext_define(system, "++", FUNC(append));
  module_ext_define(system, "tabulate", FUNC(tabulate));

  module_ext_define(system, "type-of", FUNC(type_of));

  Symbol *length_symbol = module_extern_symbol_c(system, "length");
  module_define(copy_object(length_symbol),
      GEN_FUNC(create_gen_func(copy_object(length_symbol), NULL, 1, 1, (uint8_t[]){ 0 })));
  module_define_method(system, copy_object(length_symbol),
      create_type_array(1, (Type *[]){get_poly_instance(copy_generic(vector_type))}),
      FUNC(vector_length));
  module_define_method(system, copy_object(length_symbol),
      create_type_array(1, (Type *[]){get_poly_instance(copy_generic(vector_slice_type))}),
      FUNC(vector_slice_length));
  module_define_method(system, copy_object(length_symbol),
      create_type_array(1, (Type *[]){copy_type(string_type)}),
      FUNC(string_length));
  delete_value(SYMBOL(length_symbol));

  Value stdin_val = POINTER(create_pointer(copy_type(stream_type),
        stdin_stream, void_destructor));
  Value stdout_val = POINTER(create_pointer(copy_type(stream_type),
        stdout_stream, void_destructor));
  Value stderr_val = POINTER(create_pointer(copy_type(stream_type),
        stderr_stream, void_destructor));
  module_ext_define(system, "*stdin*", stdin_val);
  module_ext_define(system, "*stdout*", stdout_val);
  module_ext_define(system, "*stderr*", stderr_val);

  module_ext_define_type(system, "any", TYPE(copy_type(any_type)));
  module_ext_define_type(system, "stream", TYPE(copy_type(stream_type)));

  return system;
}
