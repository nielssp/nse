/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include "type.h"
#include "module.h"

#include "write.h"

static void write_type(const Type *type, Stream *stream, Module *module) {
  if (!type) {
    stream_printf(stream, "#<undefined>");
    return;
  }
  Symbol *name;
  switch (type->type) {
    case TYPE_SIMPLE:
      if (type->name) {
        nse_write(SYMBOL(type->name), stream, module);
      } else {
        stream_printf(stream, "#<type>");
      }
      break;
    case TYPE_FUNC:
        stream_printf(stream, "(-> (");
        if (type->func.min_arity) {
          stream_printf(stream, "any");
          for (int i = 1; i < type->func.min_arity; i++) {
            stream_printf(stream, " any");
          }
          if (type->func.variadic) {
            stream_printf(stream, " ");
          }
        }
        if (type->func.variadic) {
          stream_printf(stream, "&rest any");
        }
        stream_printf(stream, ") any)");
        break;
    case TYPE_POLY_INSTANCE:
      stream_printf(stream, "(forall (");
      int arity = generic_type_arity(type->poly_instance);
      if (arity == 1) {
        stream_printf(stream, "t");
      } else {
        for (int i = 0; i < arity ; i++) {
          stream_printf(stream, "%st%d", i == 0 ? "" : " ", i);
        }
      }
      stream_printf(stream, ") (");
      name = generic_type_name(type->poly_instance);
      if (name) {
        nse_write(SYMBOL(name), stream, module);
      } else {
        stream_printf(stream, "#<generic-type>");
      }
      if (arity == 1) {
        stream_printf(stream, " t");
      } else {
        for (int i = 0; i < arity ; i++) {
          stream_printf(stream, " t%d", i);
        }
      }
      stream_printf(stream, "))");
      break;
    case TYPE_INSTANCE:
      stream_printf(stream, "(");
      name = generic_type_name(type->instance.type);
      if (name) {
        nse_write(SYMBOL(name), stream, module);
      } else {
        stream_printf(stream, "#<generic-type>");
      }
      TypeArray *params = type->instance.parameters;
      for (int i = 0; i < params->size; i++) {
        stream_printf(stream, " ");
        write_type(params->elements[i], stream, module);
      }
      stream_printf(stream, ")");
      break;
    case TYPE_POLY_VAR:
      stream_printf(stream, "t%d", type->poly_var.index);
      break;
  }
}

Value nse_write(const Value value, Stream *stream, Module *module) {
  switch (value.type) {
    /* Primitives */

    case VALUE_UNDEFINED:
      return undefined;
    case VALUE_UNIT:
      stream_printf(stream, "()");
      break;
    case VALUE_I64:
      stream_printf(stream, "%ld", value.i64);
      break;
    case VALUE_F64:
      stream_printf(stream, "%lf", value.f64);
      break;
    case VALUE_FUNC:
      stream_printf(stream, "#<function>");
      break;

    /* Reference types */

    case VALUE_VECTOR: {
      const Vector *v = TO_VECTOR(value);
      stream_printf(stream, "(");
      for (size_t i = 0; i < v->length; i++) {
        if (i != 0) {
          stream_printf(stream, " ");
        }
        nse_write(v->cells[i], stream, module);
      }
      stream_printf(stream, ")");
      break;
    }
    case VALUE_VECTOR_SLICE: {
      const VectorSlice *v = TO_VECTOR_SLICE(value);
      stream_printf(stream, "(");
      for (size_t i = 0; i < v->length; i++) {
        if (i != 0) {
          stream_printf(stream, " ");
        }
        nse_write(v->cells[i], stream, module);
      }
      stream_printf(stream, ")");
      break;
    }
    case VALUE_ARRAY:
    case VALUE_ARRAY_SLICE:
      return undefined;
    case VALUE_STRING: {
      const String *string = TO_STRING(value);
      stream_printf(stream, "\"");
      for (size_t i = 0; i < string->length; i++) {
        uint8_t c = string->bytes[i];
        switch (c) {
          case '"':
          case '\\':
            stream_printf(stream, "\\%c", c);
            break;
          case '\n':
            stream_printf(stream, "\\n");
            break;
          case '\r':
            stream_printf(stream, "\\r");
            break;
          case '\t':
            stream_printf(stream, "\\t");
            break;
          case '\0':
            stream_printf(stream, "\\0");
            break;
          default:
            stream_putc(c, stream);
            break;
        }
      }
      stream_printf(stream, "\"");
      break;
    }
    case VALUE_QUOTE:
      stream_printf(stream, "'");
      nse_write(TO_QUOTE(value)->quoted, stream, module);
      break;
    case VALUE_TYPE_QUOTE:
      stream_printf(stream, "^");
      nse_write(TO_QUOTE(value)->quoted, stream, module);
      break;
    case VALUE_WEAK_REF: {
      stream_printf(stream, "(weak ");
      nse_write(TO_WEAK_REF(value)->value, stream, module);
      stream_printf(stream, ")");
      break;
    }
    case VALUE_SYMBOL: {
      Symbol *symbol = TO_SYMBOL(value);
      if (module) {
        Symbol *internal = module_find_internal(module, symbol->name);
        delete_value(SYMBOL(internal));
        if (internal == symbol) {
          stream_printf(stream, "%s", TO_C_STRING(internal->name));
          break;
        }
      }
      if (symbol->module) { 
        stream_printf(stream, "%s/%s", TO_C_STRING(get_module_name(symbol->module)), TO_C_STRING(symbol->name));
      } else {
        stream_printf(stream, "#:%s", TO_C_STRING(symbol->name));
      }
      break;
    }
    case VALUE_KEYWORD:
      stream_printf(stream, ":%s", TO_C_STRING(TO_SYMBOL(value)->name));
      break;
    case VALUE_DATA: {
      Data *d = TO_DATA(value);
      if (d->size) {
        stream_printf(stream, "(");
        nse_write(SYMBOL(d->tag), stream, module);
        for (size_t i = 0; i < d->size; i++) {
          stream_printf(stream, " ");
          nse_write(d->fields[i], stream, module);
        }
        stream_printf(stream, ")");
      } else {
        nse_write(SYMBOL(d->tag), stream, module);
      }
      break;
    }
    case VALUE_SYNTAX:
      stream_printf(stream, "#<syntax ");
      nse_write(TO_SYNTAX(value)->quoted, stream, module);
      stream_printf(stream, ">");
      break;
    case VALUE_CLOSURE:
      stream_printf(stream, "#<lambda>");
      break;
    case VALUE_POINTER:
      stream_printf(stream, "#<");
      write_type(TO_POINTER(value)->type, stream, module);
      stream_printf(stream, "#%p>", TO_POINTER(value)->pointer);
      break;
    case VALUE_TYPE:
      stream_printf(stream, "^");
      write_type(TO_TYPE(value), stream, module);
      break;
  }
  return unit;
}

char *nse_write_to_string(Value value, Module *module) {
  size_t size = 32;
  char *buffer = (char *)malloc(size);
  Stream *stream = stream_buffer(buffer, size, 0);
  nse_write(value, stream, module);
  buffer = stream_get_content(stream);
  stream_close(stream);
  return buffer;
}
