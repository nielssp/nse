/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "value.h"
#include "type.h"
#include "error.h"
#include "module.h"
#include "../src/util/stream.h"

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


Module *get_system_module() {
  Module *system = create_module("system");
  module_ext_define(system, "+", FUNC(sum));

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
