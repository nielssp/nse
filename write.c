#include "write.h"

static void write_cons(Cons *cons, Stream *stream, Module *module);

static void write_cons_tail(NseVal tail, Stream *stream, Module *module) {
  if (tail.type->internal == INTERNAL_SYNTAX) {
    write_cons_tail(tail.syntax->quoted, stream, module);
  } else if (tail.type->internal == INTERNAL_CONS) {
    stream_printf(stream, " ");
    write_cons(tail.cons, stream, module);
  } else if (tail.type->internal != INTERNAL_NIL) {
    stream_printf(stream, " . ");
    nse_write(tail, stream, module);
  }
}

static void write_cons(Cons *cons, Stream *stream, Module *module) {
  nse_write(cons->head, stream, module);
  write_cons_tail(cons->tail, stream, module);
}

static void write_type(CType *type, Stream *stream, Module *module) {
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
        stream_printf(stream, "%c", value.string->chars[i]);
      }
      stream_printf(stream, "\"");
      break;
    case INTERNAL_SYMBOL: {
      if (value.type == keyword_type) {
        stream_printf(stream, ":%s", value.symbol->name);
        break;
      }
      Symbol *internal = module_find_internal(module, value.symbol->name);
      if (internal == value.symbol) {
        stream_printf(stream, "%s", value.symbol->name);
      } else {
        stream_printf(stream, "%s/%s", module_name(value.symbol->module), value.symbol->name);
      }
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
      stream_printf(stream, "#<reference#%p>", value.reference->pointer);
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
