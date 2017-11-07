#include "write.h"

static void write_cons(Cons *cons, Stream *stream, Module *module);

static void write_cons_tail(NseVal tail, Stream *stream, Module *module) {
  if (tail.type == TYPE_SYNTAX) {
    write_cons_tail(tail.syntax->quoted, stream, module);
  } else if (tail.type == TYPE_CONS) {
    stream_printf(stream, " ");
    write_cons(tail.cons, stream, module);
  } else if (tail.type != TYPE_NIL) {
    stream_printf(stream, " . ");
    nse_write(tail, stream, module);
  }
}

static void write_cons(Cons *cons, Stream *stream, Module *module) {
  nse_write(cons->head, stream, module);
  write_cons_tail(cons->tail, stream, module);
}

static void write_type(Type *type, Stream *stream, Module *module) {
  switch (type->type) {
    case BASE_TYPE_NOTHING:
    case BASE_TYPE_ANY:
    case BASE_TYPE_NIL:
    case BASE_TYPE_REF:
    case BASE_TYPE_I8:
    case BASE_TYPE_I16:
    case BASE_TYPE_I32:
    case BASE_TYPE_I64:
    case BASE_TYPE_U8:
    case BASE_TYPE_U16:
    case BASE_TYPE_U32:
    case BASE_TYPE_U64:
    case BASE_TYPE_F32:
    case BASE_TYPE_F64:
    case BASE_TYPE_STRING:
    case BASE_TYPE_ANY_SYMBOL:
    case BASE_TYPE_KEYWORD:
    case BASE_TYPE_TYPE:
      stream_printf(stream, base_type_to_string(type->type));
      break;
    case BASE_TYPE_SYMBOL:
      stream_printf(stream, "'%s", type->var_name);
      break;
    case BASE_TYPE_TYPE_VAR:
      stream_printf(stream, "%s", type->var_name);
      break;
    case BASE_TYPE_QUOTE:
    case BASE_TYPE_TYPE_QUOTE:
    case BASE_TYPE_SYNTAX:
      stream_printf(stream, "(");
      stream_printf(stream, base_type_to_string(type->type));
      stream_printf(stream, " ");
      write_type(type->param_a, stream, module);
      stream_printf(stream, ")");
      break;
    case BASE_TYPE_CONS:
      if (type_is_product(type)) {
        stream_printf(stream, "(%s", base_type_to_string(BASE_TYPE_PRODUCT));
        while (type->type == BASE_TYPE_CONS) {
          stream_printf(stream, " ");
          write_type(type->param_a, stream, module);
          type = type->param_b;
        }
        stream_printf(stream, ")");
        break;
      }
    case BASE_TYPE_FUNC:
    case BASE_TYPE_UNION:
      stream_printf(stream, "(");
      stream_printf(stream, base_type_to_string(type->type));
      stream_printf(stream, " ");
      write_type(type->param_a, stream, module);
      stream_printf(stream, " ");
      write_type(type->param_b, stream, module);
      stream_printf(stream, ")");
      break;
    case BASE_TYPE_PRODUCT:
      stream_printf(stream, "(");
      stream_printf(stream, base_type_to_string(type->type));
      for (size_t i = 0; i < type->num_operands; i++) {
        stream_printf(stream, " ");
        write_type(type->operands[i], stream, module);
      }
      stream_printf(stream, ")");
      break;
    case BASE_TYPE_RECUR:
    case BASE_TYPE_FORALL:
      stream_printf(stream, "(");
      stream_printf(stream, base_type_to_string(type->type));
      stream_printf(stream, " %s ", type->var_name);
      write_type(type->param_b, stream, module);
      stream_printf(stream, ")");
      break;
    case BASE_TYPE_ALIAS:
      stream_printf(stream, "%s", type->var_name);
      break;
  }
}

NseVal nse_write(NseVal value, Stream *stream, Module *module) {
  switch (value.type) {
    case TYPE_NIL:
      stream_printf(stream, "()");
      break;
    case TYPE_CONS:
      stream_printf(stream, "(");
      write_cons(value.cons, stream, module);
      stream_printf(stream, ")");
      break;
    case TYPE_STRING:
      stream_printf(stream, "\"");
      for (size_t i = 0; i < value.string->length; i++) {
        stream_printf(stream, "%c", value.string->chars[i]);
      }
      stream_printf(stream, "\"");
      break;
    case TYPE_SYMBOL: {
      Symbol *internal = module_find_internal(module, value.symbol->name);
      if (internal == value.symbol) {
        stream_printf(stream, "%s", value.symbol->name);
      } else {
        stream_printf(stream, "%s/%s", module_name(value.symbol->module), value.symbol->name);
      }
      break;
    }
    case TYPE_KEYWORD:
      stream_printf(stream, ":%s", value.symbol->name);
      break;
    case TYPE_I64:
      stream_printf(stream, "%ld", value.i64);
      break;
    case TYPE_F64:
      stream_printf(stream, "%lf", value.f64);
      break;
    case TYPE_QUOTE:
      stream_printf(stream, "'");
      nse_write(value.quote->quoted, stream, module);
      break;
    case TYPE_TQUOTE:
      stream_printf(stream, "^");
      nse_write(value.quote->quoted, stream, module);
      break;
    case TYPE_TYPE:
      stream_printf(stream, "^");
      write_type(value.type_val, stream, module);
      break;
    case TYPE_SYNTAX:
      stream_printf(stream, "#<syntax ");
      nse_write(value.syntax->quoted, stream, module);
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
    case TYPE_UNDEFINED:
      return undefined;
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
