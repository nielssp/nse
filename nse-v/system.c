/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "type.h"
#include "error.h"
#include "module.h"
#include "../src/util/stream.h"
#include "eval.h"
#include "lang.h"

#include "system.h"

static Value nothing_sum(Slice args, Scope *dynamic_scope) {
  delete_slice(args);
  return I64(0);
}

static Value i64_sum(Slice args, Scope *dynamic_scope) {
  int64_t acc = 0;
  for (size_t i = 0; i < args.length; i++) {
    Value h = args.cells[i];
    if (h.type == VALUE_I64) {
      acc += h.i64;
    } else {
      delete_slice(args);
      set_debug_arg_index(i);
      raise_error(domain_error, "expected i64");
      return undefined;
    }
  }
  delete_slice(args);
  return I64(acc);
}

static Value num_sum(Slice args, Scope *dynamic_scope) {
  double acc = 0.0;
  for (size_t i = 0; i < args.length; i++) {
    Value h = args.cells[i];
    if (h.type == VALUE_I64) {
      acc += h.i64;
    } else if (h.type == VALUE_F64) {
      acc += h.f64;
    } else {
      delete_slice(args);
      set_debug_arg_index(i);
      raise_error(domain_error, "expected number");
      return undefined;
    }
  }
  delete_slice(args);
  return F64(acc);
}

static Value i64_subtract(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    raise_error(domain_error, "too few parameters");
  } else if (args.cells[0].type != VALUE_I64) {
    set_debug_arg_index(0);
    raise_error(domain_error, "expected i64");
  } else {
    int64_t acc = args.cells[0].i64;
    if (args.length > 1) {
      for (size_t i = 1; i < args.length; i++) {
        Value h = args.cells[i];
        if (h.type == VALUE_I64) {
          acc -= h.i64;
        } else {
          delete_slice(args);
          set_debug_arg_index(i);
          raise_error(domain_error, "expected i64");
          return undefined;
        }
      }
      result = I64(acc);
    } else {
      result = I64(-acc);
    }
  }
  delete_slice(args);
  return result;
}

static Value num_subtract(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    raise_error(domain_error, "too few parameters");
  } else if (args.cells[0].type != VALUE_I64 && args.cells[0].type != VALUE_F64) {
    set_debug_arg_index(0);
    raise_error(domain_error, "expected number");
  } else {
    double acc = args.cells[0].type == VALUE_I64 ? args.cells[0].i64 : args.cells[0].f64;
    if (args.length > 1) {
      for (size_t i = 1; i < args.length; i++) {
        Value h = args.cells[i];
        if (h.type == VALUE_I64) {
          acc -= h.i64;
        } else if (h.type == VALUE_F64) {
          acc -= h.f64;
        } else {
          delete_slice(args);
          set_debug_arg_index(i);
          raise_error(domain_error, "expected number");
          return undefined;
        }
      }
      result = F64(acc);
    } else {
      result = F64(-acc);
    }
  }
  delete_slice(args);
  return result;
}

static Value nothing_product(Slice args, Scope *dynamic_scope) {
  delete_slice(args);
  return I64(1);
}

static Value i64_product(Slice args, Scope *dynamic_scope) {
  int64_t acc = 1;
  for (size_t i = 0; i < args.length; i++) {
    Value h = args.cells[i];
    if (h.type == VALUE_I64) {
      acc *= h.i64;
    } else {
      delete_slice(args);
      set_debug_arg_index(i);
      raise_error(domain_error, "expected i64");
      return undefined;
    }
  }
  delete_slice(args);
  return I64(acc);
}

static Value num_product(Slice args, Scope *dynamic_scope) {
  double acc = 1.0;
  for (size_t i = 0; i < args.length; i++) {
    Value h = args.cells[i];
    if (h.type == VALUE_I64) {
      acc *= h.i64;
    } else if (h.type == VALUE_F64) {
      acc *= h.f64;
    } else {
      delete_slice(args);
      set_debug_arg_index(i);
      raise_error(domain_error, "expected number");
      return undefined;
    }
  }
  delete_slice(args);
  return F64(acc);
}

static Value i64_divide(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    raise_error(domain_error, "too few parameters");
  } else if (args.cells[0].type != VALUE_I64) {
    set_debug_arg_index(0);
    raise_error(domain_error, "expected i64");
  } else {
    int64_t acc = args.cells[0].i64;
    if (args.length > 1) {
      for (size_t i = 1; i < args.length; i++) {
        Value h = args.cells[i];
        if (h.type == VALUE_I64) {
          acc /= h.i64;
        } else {
          delete_slice(args);
          set_debug_arg_index(i);
          raise_error(domain_error, "expected i64");
          return undefined;
        }
      }
      result = I64(acc);
    } else {
      result = I64(1 / acc);
    }
  }
  delete_slice(args);
  return result;
}

static Value num_divide(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    raise_error(domain_error, "too few parameters");
  } else if (args.cells[0].type != VALUE_I64 && args.cells[0].type != VALUE_F64) {
    set_debug_arg_index(0);
    raise_error(domain_error, "expected number");
  } else {
    double acc = args.cells[0].type == VALUE_I64 ? args.cells[0].i64 : args.cells[0].f64;
    if (args.length > 1) {
      for (size_t i = 1; i < args.length; i++) {
        Value h = args.cells[i];
        if (h.type == VALUE_I64) {
          acc /= h.i64;
        } else if (h.type == VALUE_F64) {
          acc /= h.f64;
        } else {
          delete_slice(args);
          set_debug_arg_index(i);
          raise_error(domain_error, "expected number");
          return undefined;
        }
      }
      result = F64(acc);
    } else {
      result = F64(1.0 / acc);
    }
  }
  delete_slice(args);
  return result;
}

static Value i64_less_than(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    raise_error(domain_error, "too few parameters");
  } else if (args.cells[0].type != VALUE_I64) {
    set_debug_arg_index(0);
    raise_error(domain_error, "expected i64");
  } else {
    int64_t previous = args.cells[0].i64;
    if (args.length > 1) {
      for (size_t i = 1; i < args.length; i++) {
        Value h = args.cells[i];
        if (h.type == VALUE_I64) {
          if (h.i64 > previous) {
            previous = h.i64;
          } else {
            delete_slice(args);
            return FALSE;
          }
        } else {
          delete_slice(args);
          set_debug_arg_index(i);
          raise_error(domain_error, "expected i64");
          return undefined;
        }
      }
    }
    result = TRUE;
  }
  delete_slice(args);
  return result;
}

static Value num_less_than(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    raise_error(domain_error, "too few parameters");
  } else if (args.cells[0].type != VALUE_I64 && args.cells[0].type != VALUE_F64) {
    set_debug_arg_index(0);
    raise_error(domain_error, "expected number");
  } else {
    double previous = args.cells[0].type == VALUE_I64 ? args.cells[0].i64 : args.cells[0].f64;
    if (args.length > 1) {
      for (size_t i = 1; i < args.length; i++) {
        Value h = args.cells[i];
        if (h.type == VALUE_I64) {
          if (h.i64 > previous) {
            previous = h.i64;
          } else {
            delete_slice(args);
            return FALSE;
          }
        } else if (h.type == VALUE_F64) {
          if (h.f64 > previous) {
            previous = h.f64;
          } else {
            delete_slice(args);
            return FALSE;
          }
        } else {
          delete_slice(args);
          set_debug_arg_index(i);
          raise_error(domain_error, "expected number");
          return undefined;
        }
      }
    }
    result = TRUE;
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

static Value string_elem(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64
      && args.cells[1].type == VALUE_STRING) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_STRING(args.cells[1])->length) {
      result = I64(TO_STRING(args.cells[1])->bytes[args.cells[0].i64]);
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (elem INT STRING)");
  }
  delete_slice(args);
  return result;
}

static Value vector_elem(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64
      && args.cells[1].type == VALUE_VECTOR) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_VECTOR(args.cells[1])->length) {
      result = copy_value(TO_VECTOR(args.cells[1])->cells[args.cells[0].i64]);
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (elem INT VECTOR)");
  }
  delete_slice(args);
  return result;
}

static Value vector_slice_elem(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64
      && args.cells[1].type == VALUE_VECTOR_SLICE) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_VECTOR_SLICE(args.cells[1])->length) {
      result = copy_value(TO_VECTOR_SLICE(args.cells[1])->cells[args.cells[0].i64]);
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (elem INT VECTOR-SLICE)");
  }
  delete_slice(args);
  return result;
}


Module *get_system_module() {
  Module *system = create_module("system");

  module_ext_define(system, "++", FUNC(append));
  module_ext_define(system, "tabulate", FUNC(tabulate));

  module_ext_define(system, "type-of", FUNC(type_of));

  module_ext_define_generic(system, "+", 0, 1, 1, (int8_t[]){ 0 });
  module_ext_define_method(system, "+", FUNC(nothing_sum),
      1, copy_type(nothing_type));
  module_ext_define_method(system, "+", FUNC(i64_sum),
      1, copy_type(i64_type));
  module_ext_define_method(system, "+", FUNC(num_sum),
      1, copy_type(num_type));

  module_ext_define_generic(system, "-", 1, 1, 1, (int8_t[]){ 0, 0 });
  module_ext_define_method(system, "-", FUNC(i64_subtract),
      1, copy_type(i64_type));
  module_ext_define_method(system, "-", FUNC(num_subtract),
      1, copy_type(num_type));

  module_ext_define_generic(system, "*", 0, 1, 1, (int8_t[]){ 0 });
  module_ext_define_method(system, "*", FUNC(nothing_product),
      1, copy_type(nothing_type));
  module_ext_define_method(system, "*", FUNC(i64_product),
      1, copy_type(i64_type));
  module_ext_define_method(system, "*", FUNC(num_product),
      1, copy_type(num_type));

  module_ext_define_generic(system, "/", 1, 1, 1, (int8_t[]){ 0, 0 });
  module_ext_define_method(system, "/", FUNC(i64_divide),
      1, copy_type(i64_type));
  module_ext_define_method(system, "/", FUNC(num_divide),
      1, copy_type(num_type));

  module_ext_define_generic(system, "<", 1, 1, 1, (int8_t[]){ 0, 0 });
  module_ext_define_method(system, "<", FUNC(i64_less_than),
      1, copy_type(i64_type));
  module_ext_define_method(system, "<", FUNC(num_less_than),
      1, copy_type(num_type));

  module_ext_define_generic(system, "length", 1, 0, 1, (int8_t[]){ 0 });
  module_ext_define_method(system, "length", FUNC(vector_length),
      1, get_poly_instance(copy_generic(vector_type)));
  module_ext_define_method(system, "length", FUNC(vector_slice_length),
      1, get_poly_instance(copy_generic(vector_slice_type)));
  module_ext_define_method(system, "length", FUNC(string_length),
      1, copy_type(string_type));

  module_ext_define_generic(system, "elem", 2, 0, 1, (int8_t[]){ -1, 0 });
  module_ext_define_method(system, "elem", FUNC(vector_elem),
      1, get_poly_instance(copy_generic(vector_type)));
  module_ext_define_method(system, "elem", FUNC(vector_slice_elem),
      1, get_poly_instance(copy_generic(vector_slice_type)));
  module_ext_define_method(system, "elem", FUNC(string_elem),
      1, copy_type(string_type));

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
  module_ext_define_type(system, "num", TYPE(copy_type(num_type)));
  module_ext_define_type(system, "int", TYPE(copy_type(int_type)));
  module_ext_define_type(system, "float", TYPE(copy_type(float_type)));
  module_ext_define_type(system, "i64", TYPE(copy_type(i64_type)));
  module_ext_define_type(system, "f64", TYPE(copy_type(f64_type)));
  module_ext_define_type(system, "stream", TYPE(copy_type(stream_type)));

  return system;
}
