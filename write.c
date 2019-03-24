#include "write.h"

static void write_cons(Cons *cons, Stream *stream, Module *module) {
  while (cons) {
    nse_write(cons->head, stream, module);
    NseVal tail = cons->tail;
    while (tail.type->internal == INTERNAL_SYNTAX) {
      tail = tail.syntax->quoted;
    }
    if (tail.type->internal == INTERNAL_CONS) {
      stream_printf(stream, " ");
      cons = tail.cons;
    } else {
      if (tail.type->internal != INTERNAL_NIL) {
        stream_printf(stream, " . ");
        nse_write(tail, stream, module);
      }
      break;
    }
  }
}

static void write_type(CType *type, Stream *stream, Module *module) {
  switch (type->type) {
    case C_TYPE_SIMPLE:
      if (type->name) {
        nse_write(SYMBOL(type->name), stream, module);
      } else {
        stream_printf(stream, "#<type>");
      }
      break;
    case C_TYPE_FUNC:
    case C_TYPE_CLOSURE:
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
    case C_TYPE_INSTANCE:
      stream_printf(stream, "(");
      Symbol *name = generic_type_name(type->instance.type);
      if (name) {
        nse_write(SYMBOL(name), stream, module);
      } else {
        stream_printf(stream, "#<generic-type>");
      }
      CType **param = type->instance.parameters;
      while (*param) {
        stream_printf(stream, " ");
        write_type(*(param++), stream, module);
      }
      stream_printf(stream, ")");
      break;
  }
}

NseVal nse_write(NseVal value, Stream *stream, Module *module) {
  if (!value.type) {
    return undefined;
  }
  switch (value.type->internal) {
    case INTERNAL_NIL:
      stream_printf(stream, "()");
      break;
    case INTERNAL_CONS:
      stream_printf(stream, "(");
      write_cons(value.cons, stream, module);
      stream_printf(stream, ")");
      break;
    case INTERNAL_STRING:
      stream_printf(stream, "\"");
      for (size_t i = 0; i < value.string->length; i++) {
        char c = value.string->chars[i];
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
    case INTERNAL_SYMBOL: {
      if (value.type == keyword_type) {
        stream_printf(stream, ":%s", value.symbol->name);
        break;
      }
      if (module) {
        Symbol *internal = module_find_internal(module, value.symbol->name);
        if (internal == value.symbol) {
          stream_printf(stream, "%s", value.symbol->name);
          break;
        }
      } 
      stream_printf(stream, "%s/%s", module_name(value.symbol->module), value.symbol->name);
      break;
    }
    case INTERNAL_I64:
      stream_printf(stream, "%ld", value.i64);
      break;
    case INTERNAL_F64:
      stream_printf(stream, "%lf", value.f64);
      break;
    case INTERNAL_QUOTE:
      if (value.type == type_quote_type) {
        stream_printf(stream, "^");
        nse_write(value.quote->quoted, stream, module);
        break;
      } else if (value.type == continue_type) {
        stream_printf(stream, "#<continue ");
        nse_write(value.quote->quoted, stream, module);
        stream_printf(stream, ">");
        break;
      }
      stream_printf(stream, "'");
      nse_write(value.quote->quoted, stream, module);
      break;
    case INTERNAL_TYPE:
      stream_printf(stream, "^");
      write_type(value.type_val, stream, module);
      break;
    case INTERNAL_SYNTAX:
      stream_printf(stream, "#<syntax ");
      nse_write(value.syntax->quoted, stream, module);
      stream_printf(stream, ">");
      break;
    case INTERNAL_FUNC:
      stream_printf(stream, "#<function>");
      break;
    case INTERNAL_CLOSURE:
      stream_printf(stream, "#<lambda>");
      break;
    case INTERNAL_REFERENCE:
      stream_printf(stream, "#<");
      write_type(value.type, stream, module);
      stream_printf(stream, "#%p>", value.reference->pointer);
      break;
    case INTERNAL_NOTHING:
      break;
  }
  return nil;
}

char *nse_write_to_string(NseVal value, Module *module) {
  size_t size = 32;
  char *buffer = (char *)malloc(size);
  Stream *stream = stream_buffer(buffer, size);
  nse_write(value, stream, module);
  buffer = stream_get_content(stream);
  stream_close(stream);
  return buffer;
}
