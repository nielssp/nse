/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <errno.h>
#include <string.h>

#include "value.h"
#include "type.h"
#include "error.h"
#include "module.h"
#include "../src/util/stream.h"
#include "eval.h"
#include "lang.h"
#include "read.h"
#include "write.h"
#include "special.h"

#include "system.h"

static Module *system_module = NULL;

static Value load(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_STRING) {
    const char *name = TO_C_STRING(TO_STRING(args.cells[0]));
    Module *m = dynamic_scope->module;
    Stream *f = stream_file(name, "r");
    if (f) {
      Reader *reader = open_reader(f, name, dynamic_scope->module);
      return_value = unit;
      while (1) {
        set_reader_module(reader, dynamic_scope->module);
        Syntax *code = nse_read(reader);
        if (code != NULL) {
          Value result = eval(SYNTAX(code), dynamic_scope);
          if (RESULT_OK(result)) {
            delete_value(result);
          } else {
            return_value = undefined;
            break;
          }
        } else {
          // TODO: check type of error
          clear_error();
          break;
        }
      }
      close_reader(reader);
      dynamic_scope->module = m;
    } else {
      raise_error(io_error, "could not open file: %s: %s", name, strerror(errno));
    }
  } else {
    raise_error(domain_error, "expected (load STRING)");
  }
  delete_slice(args);
  return return_value;
}

static Value read(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_STRING) {
    String *string = TO_STRING(args.cells[0]);
    Stream *input = stream_buffer((char *)string->bytes, string->length, string->length);
    Reader *reader = open_reader(input, "(read)", dynamic_scope->module);
    return_value = check_alloc(SYNTAX(nse_read(reader)));
    close_reader(reader);
  } else {
    raise_error(domain_error, "expected (read STRING)");
  }
  delete_slice(args);
  return return_value;
}

static Value eval_(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1) {
    return_value = eval(copy_value(args.cells[0]), dynamic_scope);
  } else {
    raise_error(domain_error, "expected (eval ANY)");
  }
  delete_slice(args);
  return return_value;
}

static Value write(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1) {
    char *buffer = allocate(32);
    if (buffer) {
      Stream *output = stream_buffer(buffer, 32, 0);
      Value result = nse_write(args.cells[0], output, dynamic_scope->module, 500);
      buffer = stream_get_content(output);
      if (RESULT_OK(result)) {
        return_value = check_alloc(STRING(create_string((uint8_t *)buffer, stream_get_size(output))));
      }
      stream_close(output);
      free(buffer);
    }
  } else {
    raise_error(domain_error, "expected (write ANY)");
  }
  delete_slice(args);
  return return_value;
}

static Value def_module(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1 && syntax_is_string_like(args.cells[0])) {
    String *name = syntax_get_string(args.cells[0]);
    Module *m = find_module(name);
    if (!m) {
      m = create_module(TO_C_STRING(name));
      if (m) {
        import_module(m, lang_module);
        import_module(m, system_module);
        dynamic_scope->module = m;
        return_value = TRUE;
      }
    } else {
      dynamic_scope->module = m;
      return_value = FALSE;
    }
    delete_value(STRING(name));
  } else {
    raise_error(domain_error, "expected (def-module STRING-LIKE)");
  }
  delete_slice(args);
  return return_value;
}

static Value in_module(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1 && syntax_is_string_like(args.cells[0])) {
    String *name = syntax_get_string(args.cells[0]);
    Module *m = find_module(name);
    if (m) {
      dynamic_scope->module = m;
      return_value = unit;
    } else {
      raise_error(name_error, "could not find module: %s", TO_C_STRING(name));
    }
    delete_value(STRING(name));
  } else {
    raise_error(domain_error, "expected (in-module STRING-LIKE)");
  }
  delete_slice(args);
  return return_value;
}

static Value export(Slice args, Scope *dynamic_scope) {
  Value return_value = unit;
  for (int i = 0; i < args.length; i++) {
    if (args.cells[i].type == VALUE_SYMBOL) {
      Symbol *symbol = TO_SYMBOL(args.cells[i]);
      Value result = check_alloc(SYMBOL(module_extern_symbol(dynamic_scope->module, copy_object(symbol->name))));
      if (!RESULT_OK(result)) {
        return_value = result;
        break;
      }
    } else {
      set_debug_arg_index(i);
      raise_error(domain_error, "expected SYMBOL");
      return_value = undefined;
      break;
    }
  }
  delete_slice(args);
  return return_value;
}

static Value import(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1 && syntax_is_string_like(args.cells[0])) {
    String *name = syntax_get_string(args.cells[0]);
    Module *m = find_module(name);
    if (m) {
      import_module(dynamic_scope->module, m);
      return_value = unit;
    } else {
      raise_error(name_error, "could not find module: %s", TO_C_STRING(name));
    }
    delete_value(STRING(name));
  } else {
    raise_error(domain_error, "expected (import STRING-LIKE)");
  }
  delete_slice(args);
  return return_value;
}

static Value namespace(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_SYMBOL) {
    Symbol *namespace_name = TO_SYMBOL(args.cells[0]);
    return_value = check_alloc(HASH_MAP(get_namespace(dynamic_scope->module, namespace_name)));
  } else {
    raise_error(domain_error, "expected (namespace SYMBOL)");
  }
  delete_slice(args);
  return return_value;
}

static Value symbol_name(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_SYMBOL) {
    return_value = copy_value(STRING(TO_SYMBOL(args.cells[0])->name));
  } else {
    raise_error(domain_error, "expected (symbol-name SYMBOL)");
  }
  delete_slice(args);
  return return_value;
}

static Value symbol_module(Slice args, Scope *dynamic_scope) {
  Value return_value = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_SYMBOL) {
    return_value = copy_value(STRING(get_module_name(TO_SYMBOL(args.cells[0])->module)));
  } else {
    raise_error(domain_error, "expected (symbol-module SYMBOL)");
  }
  delete_slice(args);
  return return_value;
}


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

static Value any_equals(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    raise_error(domain_error, "too few parameters");
  } else {
    for (size_t i = 1; i < args.length; i++) {
      Equality eq = equals(args.cells[0], args.cells[i]);
      if (eq == EQ_NOT_EQUAL) {
        delete_slice(args);
        return FALSE;
      } else if (eq == EQ_ERROR) {
        delete_slice(args);
        return undefined;
      }
    }
    result = TRUE;
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
    length += get_slice_length(args.cells[i]);
  }
  Vector *result = create_vector(length);
  size_t result_i = 0;
  for (size_t i = 0; i < args.length; i++) {
    Slice slice = to_slice(copy_value(args.cells[i]));
    for (size_t j = 0; j < slice.length; j++) {
      result->cells[result_i++] = copy_value(slice.cells[j]);
    }
    delete_slice(slice);
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

static Value tabulate_array(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64) {
    Value function = args.cells[1];
    int64_t length = args.cells[0].i64;
    Array *array = create_array(length);
    if (array) {
      int ok = 1;
      for (size_t i = 0; i < length; i++) {
        Value element = apply(copy_value(function), to_slice(I64(i)), dynamic_scope);
        if (RESULT_OK(element)) {
          array->cells[i] = element;
        } else {
          set_debug_arg_index(1);
          ok = 0;
          break;
        }
      }
      if (ok) {
        result = ARRAY(array);
      } else {
        delete_value(ARRAY(array));
      }
    }
  } else {
    raise_error(domain_error, "expected (tabulate-array INT FUNCTION)");
  }
  delete_slice(args);
  return result;
}

static Value apply_(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2) {
    result = apply(copy_value(args.cells[0]), to_slice(copy_value(args.cells[1])), dynamic_scope);
  } else {
    raise_error(domain_error, "expected (apply ANY ANY)");
  }
  delete_slice(args);
  return result;
}

static Value weak(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1) {
    result = check_alloc(WEAK_REF(create_weak_ref(copy_value(args.cells[0]))));
  } else {
    raise_error(domain_error, "expected (weak ANY)");
  }
  delete_slice(args);
  return result;
}

static Value array_buffer(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    result = check_alloc(ARRAY_BUFFER(create_array_buffer(0)));
  } else if (args.length == 1 && args.cells[0].type == VALUE_I64) {
    result = check_alloc(ARRAY_BUFFER(create_array_buffer(args.cells[0].i64)));
  } else {
    raise_error(domain_error, "expected (array-buffer [INT])");
  }
  delete_slice(args);
  return result;
}

static Value hash_map(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 0) {
    result = check_alloc(HASH_MAP(create_hash_map()));
  } else {
    raise_error(domain_error, "expected (hash-map)");
  }
  delete_slice(args);
  return result;
}

static Value hash_of(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1) {
    result = I64(hash(INIT_HASH, args.cells[0]));
  } else {
    raise_error(domain_error, "expected (hash-of ANY)");
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

static Value is_a(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[1].type == VALUE_TYPE) {
    Type *type_a = get_type(args.cells[0]);
    Type *type_b = TO_TYPE(args.cells[1]);
    if (type_a) {
      result = is_subtype_of(type_a, type_b) ? TRUE : FALSE;
      delete_type(type_a);
    }
  } else {
    raise_error(domain_error, "expected (is-a ANY TYPE)");
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

static Value string_get(Slice args, Scope *dynamic_scope) {
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
    raise_error(domain_error, "expected (get INT STRING)");
  }
  delete_slice(args);
  return result;
}

static Value vector_get(Slice args, Scope *dynamic_scope) {
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
    raise_error(domain_error, "expected (get INT VECTOR)");
  }
  delete_slice(args);
  return result;
}

static Value vector_slice_get(Slice args, Scope *dynamic_scope) {
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
    raise_error(domain_error, "expected (get INT VECTOR-SLICE)");
  }
  delete_slice(args);
  return result;
}

static Value array_get(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64
      && args.cells[1].type == VALUE_ARRAY) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_ARRAY(args.cells[1])->length) {
      result = copy_value(TO_ARRAY(args.cells[1])->cells[args.cells[0].i64]);
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (get INT ARRAY)");
  }
  delete_slice(args);
  return result;
}

static Value array_slice_get(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64
      && args.cells[1].type == VALUE_ARRAY_SLICE) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_ARRAY_SLICE(args.cells[1])->length) {
      result = copy_value(TO_ARRAY_SLICE(args.cells[1])->cells[args.cells[0].i64]);
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (get INT ARRAY-SLICE)");
  }
  delete_slice(args);
  return result;
}

static Value array_buffer_get(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64
      && args.cells[1].type == VALUE_ARRAY_BUFFER) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_ARRAY_BUFFER(args.cells[1])->length) {
      result = copy_value(TO_ARRAY_BUFFER(args.cells[1])->cells[args.cells[0].i64]);
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (get INT ARRAY-BUFFER)");
  }
  delete_slice(args);
  return result;
}

static Value hash_map_get_(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[1].type == VALUE_HASH_MAP) {
    result = hash_map_get(copy_object(TO_HASH_MAP(args.cells[1])), copy_value(args.cells[0]));
  } else {
    raise_error(domain_error, "expected (get ANY HASH-MAP)");
  }
  delete_slice(args);
  return result;
}

static Value slice_(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 3 && args.cells[0].type == VALUE_I64
      && args.cells[1].type == VALUE_I64
      && (args.cells[2].type == VALUE_VECTOR
        || args.cells[2].type == VALUE_VECTOR_SLICE
        || args.cells[2].type == VALUE_ARRAY
        || args.cells[2].type == VALUE_ARRAY_SLICE)) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_ARRAY(args.cells[2])->length) {
      if (args.cells[1].i64 >= 0
          && args.cells[0].i64 + args.cells[1].i64 <= TO_ARRAY(args.cells[2])->length) {
        result = check_alloc(slice_to_value(slice(copy_value(args.cells[2]), args.cells[0].i64, args.cells[1].i64)));
      } else {
        set_debug_arg_index(1);
        raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64 + args.cells[1].i64);
      }
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (slice INT INT ARRAY)");
  }
  delete_slice(args);
  return result;
}

static Value array_put(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 3 && args.cells[0].type == VALUE_I64
      && args.cells[2].type == VALUE_ARRAY) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_ARRAY(args.cells[2])->length) {
      result = array_set(TO_ARRAY(copy_value(args.cells[2])), args.cells[0].i64, copy_value(args.cells[1]));
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (put INT ANY ARRAY)");
  }
  delete_slice(args);
  return result;
}

static Value array_slice_put(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 3 && args.cells[0].type == VALUE_I64
      && args.cells[2].type == VALUE_ARRAY_SLICE) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_ARRAY_SLICE(args.cells[2])->length) {
      result = array_slice_set(TO_ARRAY_SLICE(copy_value(args.cells[2])), args.cells[0].i64, copy_value(args.cells[1]));
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (put INT ANY ARRAY-SLICE)");
  }
  delete_slice(args);
  return result;
}

static Value array_buffer_put(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 3 && args.cells[0].type == VALUE_I64
      && args.cells[2].type == VALUE_ARRAY_BUFFER) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_ARRAY_BUFFER(args.cells[2])->length) {
      result = array_buffer_set(TO_ARRAY_BUFFER(copy_value(args.cells[2])), args.cells[0].i64, copy_value(args.cells[1]));
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (put INT ANY ARRAY-BUFFER)");
  }
  delete_slice(args);
  return result;
}

static Value hash_map_put(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 3 && args.cells[2].type == VALUE_HASH_MAP) {
    result = hash_map_set(copy_object(TO_HASH_MAP(args.cells[2])), copy_value(args.cells[0]),
        copy_value(args.cells[1]));
  } else {
    raise_error(domain_error, "expected (put ANY ANY HASH-MAP)");
  }
  delete_slice(args);
  return result;
}

static Value array_buffer_delete_(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_I64
      && args.cells[1].type == VALUE_ARRAY_BUFFER) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 < TO_ARRAY_BUFFER(args.cells[1])->length) {
      result = array_buffer_delete(copy_object(TO_ARRAY_BUFFER(args.cells[1])), args.cells[0].i64);
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (delete INT ARRAY-BUFFER)");
  }
  delete_slice(args);
  return result;
}

static Value hash_map_delete(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[1].type == VALUE_HASH_MAP) {
    result = hash_map_unset(copy_object(TO_HASH_MAP(args.cells[1])), copy_value(args.cells[0]));
  } else {
    raise_error(domain_error, "expected (delete ANY HASH-MAP)");
  }
  delete_slice(args);
  return result;
}

static Value array_buffer_insert_(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 3 && args.cells[0].type == VALUE_I64
      && args.cells[2].type == VALUE_ARRAY_BUFFER) {
    if (args.cells[0].i64 >= 0
        && args.cells[0].i64 <= TO_ARRAY_BUFFER(args.cells[2])->length) {
      result = check_alloc(ARRAY_BUFFER(array_buffer_insert(copy_object(TO_ARRAY_BUFFER(args.cells[2])), args.cells[0].i64, copy_value(args.cells[1]))));
    } else {
      set_debug_arg_index(0);
      raise_error(domain_error, "index out of bounds: %ld", args.cells[0].i64);
    }
  } else {
    raise_error(domain_error, "expected (insert INT ANY ARRAY-BUFFER)");
  }
  delete_slice(args);
  return result;
}

static Value syntax_to_datum_(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1) {
    result = syntax_to_datum(copy_value(args.cells[0]));
  } else {
    raise_error(domain_error, "expected (syntax->datum ANY)");
  }
  delete_slice(args);
  return result;
}

static Value get_weak_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 1 && args.cells[0].type == VALUE_TYPE) {
    result = TYPE(get_unary_instance(copy_generic(weak_ref_type), copy_type(TO_TYPE(args.cells[0]))));
  } else {
    raise_error(domain_error, "expected (weak TYPE)");
  }
  delete_slice(args);
  return result;
}

static Value get_hash_map_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_TYPE && args.cells[1].type == VALUE_TYPE) {
    TypeArray *a = create_type_array(2, (Type *[]){ TO_TYPE(args.cells[0]), TO_TYPE(args.cells[1]) });
    if (a) {
      result = TYPE(get_instance(copy_generic(hash_map_type), a));
    }
  } else {
    raise_error(domain_error, "expected (hash-map TYPE TYPE)");
  }
  delete_slice(args);
  return result;
}

static Value get_entry_type(Slice args, Scope *dynamic_scope) {
  Value result = undefined;
  if (args.length == 2 && args.cells[0].type == VALUE_TYPE && args.cells[1].type == VALUE_TYPE) {
    TypeArray *a = create_type_array(2, (Type *[]){ TO_TYPE(args.cells[0]), TO_TYPE(args.cells[1]) });
    if (a) {
      result = TYPE(get_instance(copy_generic(entry_type), a));
    }
  } else {
    raise_error(domain_error, "expected (entry TYPE TYPE)");
  }
  delete_slice(args);
  return result;
}

Module *get_system_module(void) {
  if (system_module) {
    return system_module;
  }
  Module *system = create_module("system");
  import_module(system, lang_module);

  module_ext_define(system, "load", FUNC(load));
  module_ext_define(system, "read", FUNC(read));
  module_ext_define(system, "eval", FUNC(eval_));
  module_ext_define(system, "write", FUNC(write));
  module_ext_define(system, "def-module", FUNC(def_module));
  module_ext_define(system, "in-module", FUNC(in_module));
  module_ext_define(system, "export", FUNC(export));
  module_ext_define(system, "import", FUNC(import));
  module_ext_define(system, "namespace", FUNC(namespace));

  module_ext_define(system, "symbol-name", FUNC(symbol_name));
  module_ext_define(system, "symbol-module", FUNC(symbol_module));

  module_ext_define(system, "++", FUNC(append));
  module_ext_define(system, "tabulate", FUNC(tabulate));
  module_ext_define(system, "tabulate-array", FUNC(tabulate_array));
  module_ext_define(system, "apply", FUNC(apply_));
  module_ext_define(system, "weak", FUNC(weak));
  module_ext_define(system, "array-buffer", FUNC(array_buffer));
  module_ext_define(system, "hash-map", FUNC(hash_map));
  module_ext_define(system, "hash-of", FUNC(hash_of));

  module_ext_define(system, "type-of", FUNC(type_of));
  module_ext_define(system, "is-a", FUNC(is_a));

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

  module_ext_define_generic(system, "=", 1, 1, 1, (int8_t[]){ 0, 0 });
  module_ext_define_method(system, "=", FUNC(any_equals),
      1, copy_type(any_type));

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

  module_ext_define_generic(system, "get", 2, 0, 1, (int8_t[]){ -1, 0 });
  module_ext_define_method(system, "get", FUNC(vector_get),
      1, get_poly_instance(copy_generic(vector_type)));
  module_ext_define_method(system, "get", FUNC(vector_slice_get),
      1, get_poly_instance(copy_generic(vector_slice_type)));
  module_ext_define_method(system, "get", FUNC(array_get),
      1, get_poly_instance(copy_generic(array_type)));
  module_ext_define_method(system, "get", FUNC(array_slice_get),
      1, get_poly_instance(copy_generic(array_slice_type)));
  module_ext_define_method(system, "get", FUNC(array_buffer_get),
      1, get_poly_instance(copy_generic(array_buffer_type)));
  module_ext_define_method(system, "get", FUNC(string_get),
      1, copy_type(string_type));
  module_ext_define_method(system, "get", FUNC(hash_map_get_),
      1, get_poly_instance(copy_generic(hash_map_type)));

  module_ext_define_generic(system, "slice", 3, 0, 1, (int8_t[]){ -1, -1, 0 });
  module_ext_define_method(system, "slice", FUNC(slice_),
      1, get_poly_instance(copy_generic(vector_type)));
  module_ext_define_method(system, "slice", FUNC(slice_),
      1, get_poly_instance(copy_generic(vector_slice_type)));
  module_ext_define_method(system, "slice", FUNC(slice_),
      1, get_poly_instance(copy_generic(array_type)));
  module_ext_define_method(system, "slice", FUNC(slice_),
      1, get_poly_instance(copy_generic(array_slice_type)));

  module_ext_define_generic(system, "put", 3, 0, 1, (int8_t[]){ -1, -1, 0 });
  module_ext_define_method(system, "put", FUNC(array_put),
      1, get_poly_instance(copy_generic(array_type)));
  module_ext_define_method(system, "put", FUNC(array_slice_put),
      1, get_poly_instance(copy_generic(array_slice_type)));
  module_ext_define_method(system, "put", FUNC(array_buffer_put),
      1, get_poly_instance(copy_generic(array_buffer_type)));
  module_ext_define_method(system, "put", FUNC(hash_map_put),
      1, get_poly_instance(copy_generic(hash_map_type)));

  module_ext_define_generic(system, "delete", 2, 0, 1, (int8_t[]){ -1, 0 });
  module_ext_define_method(system, "delete", FUNC(array_buffer_delete_),
      1, get_poly_instance(copy_generic(array_buffer_type)));
  module_ext_define_method(system, "delete", FUNC(hash_map_delete),
      1, get_poly_instance(copy_generic(hash_map_type)));

  module_ext_define_generic(system, "insert", 3, 0, 1, (int8_t[]){ -1, -1, 0 });
  module_ext_define_method(system, "insert", FUNC(array_buffer_insert_),
      1, get_poly_instance(copy_generic(array_buffer_type)));

  module_ext_define(system, "syntax->datum", FUNC(syntax_to_datum_));

  Value stdin_val = POINTER(create_pointer(copy_type(stream_type),
        stdin_stream, void_destructor));
  Value stdout_val = POINTER(create_pointer(copy_type(stream_type),
        stdout_stream, void_destructor));
  Value stderr_val = POINTER(create_pointer(copy_type(stream_type),
        stderr_stream, void_destructor));
  module_ext_define(system, "*stdin*", stdin_val);
  module_ext_define(system, "*stdout*", stdout_val);
  module_ext_define(system, "*stderr*", stderr_val);

  set_generic_type_name(weak_ref_type, module_ext_define_type(system, "weak", FUNC(get_weak_type)));
  set_generic_type_name(hash_map_type, module_ext_define_type(system, "hash-map", FUNC(get_hash_map_type)));
  set_generic_type_name(entry_type, module_ext_define_type(system, "entry", FUNC(get_entry_type)));

  init_special();

  Scope *scope = use_module(system);
  delete_value(load(to_slice(STRING(c_string_to_string("system.lisp"))), scope));
  scope_pop(scope);

  return system_module = system;
}
