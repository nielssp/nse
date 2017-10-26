#include "write.h"

static void write_cons(Cons *cons, Stream *stream);

static void write_cons_tail(NseVal tail, Stream *stream) {
  if (tail.type == TYPE_SYNTAX) {
    write_cons_tail(tail.syntax->quoted, stream);
  } else if (tail.type == TYPE_CONS) {
    stream_printf(stream, " ");
    write_cons(tail.cons, stream);
  } else if (tail.type != TYPE_NIL) {
    stream_printf(stream, " . ");
    nse_write(tail, stream);
  }
}

static void write_cons(Cons *cons, Stream *stream) {
  nse_write(cons->head, stream);
  write_cons_tail(cons->tail, stream);
}

NseVal nse_write(NseVal value, Stream *stream) {
  switch (value.type) {
    case TYPE_NIL:
      stream_printf(stream, "()");
      break;
    case TYPE_CONS:
      stream_printf(stream, "(");
      write_cons(value.cons, stream);
      stream_printf(stream, ")");
      break;
    case TYPE_STRING:
      stream_printf(stream, "\"");
      for (size_t i = 0; i < value.string->length; i++) {
        stream_printf(stream, "%c", value.string->chars[i]);
      }
      stream_printf(stream, "\"");
      break;
    case TYPE_SYMBOL:
      stream_printf(stream, "%s", value.symbol);
      break;
    case TYPE_I64:
      stream_printf(stream, "%ld", value.i64);
      break;
    case TYPE_QUOTE:
      stream_printf(stream, "'");
      nse_write(value.quote->quoted, stream);
      break;
    case TYPE_TQUOTE:
      stream_printf(stream, "&");
      nse_write(value.quote->quoted, stream);
      break;
    case TYPE_SYNTAX:
      stream_printf(stream, "#<syntax ");
      nse_write(value.syntax->quoted, stream);
      stream_printf(stream, ">");
      break;
    case TYPE_FUNC:
      stream_printf(stream, "#<function>");
      break;
    case TYPE_CLOSURE:
      stream_printf(stream, "#<lambda>");
      break;
    case TYPE_REFERENCE:
      stream_printf(stream, "#<reference#%p>", value.reference->pointer);
      break;
    default:
      raise_error("undefined type: %d", value.type);
      return undefined;
  }
  return nil;
}
