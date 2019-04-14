/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "value.h"
#include "type.h"
#include "error.h"
#include "lang.h"
#include "module.h"
#include "../src/util/stream.h"
#include "read.h"

#define MAX_LOOKAHEAD 2

struct Reader {
  char type;
  Stream *stream;
  String *file_name;
  size_t la;
  char la_buffer[MAX_LOOKAHEAD];
  size_t line;
  size_t column;
  Module *module;
};

typedef enum {
  SYMBOL_KEYWORD,
  SYMBOL_INTERNED,
  SYMBOL_UNINTERNED,
} SymbolType;

static Vector *read_vector(Reader *input, size_t length);

Reader *open_reader(Stream *stream, const char *file_name, Module *module) {
  Reader *s = malloc(sizeof(Reader));
  s->stream = stream;
  s->la = 0;
  s->line = 1;
  s->column = 1;
  s->file_name = c_string_to_string(file_name);
  s->module = module;
  return s;
}

void set_reader_module(Reader *reader, Module *module) {
  reader->module = module;
}

void set_reader_position(Reader *reader, size_t line, size_t column) {
  reader->line = line;
  reader->column = column;
}

void get_reader_position(Reader *reader, String **file_name, size_t *line, size_t *column) {
  if (file_name) {
    *file_name = reader->file_name;
  }
  if (line) {
    *line = reader->line;
  }
  if (column) {
    *column = reader->column;
  }
}

void close_reader(Reader *reader) {
  stream_close(reader->stream);
  delete_value(STRING(reader->file_name));
  free(reader);
}

static int pop(Reader *s) {
  int c;
  if (s->la > 0) {
    c = s->la_buffer[0];
    s->la--;
    for (int i =0; i < s->la; i++) {
      s->la_buffer[i] = s->la_buffer[i + 1];
    }
  } else {
    c = stream_getc(s->stream);
  }
  if (c == '\n') {
    s->line++;
    s->column = 1;
  } else {
    s->column++;
  }
  return c;
}

static int peekn(size_t n, Reader *s) {
  while (s->la < n) {
    int c = stream_getc(s->stream);
    if (c == EOF) {
      return EOF;
    }
    s->la_buffer[s->la] = (char)c;
    s->la++;
  }
  return s->la_buffer[n - 1];
}

static char peek(Reader *s) {
  return peekn(1, s);
}

int iswhite(int c) {
  return c == '\n' || c == '\r' || c == '\t' || c == ' ';
}

static void skip(Reader *input) {
  while (iswhite(peek(input))) {
    pop(input);
  }
}

static void delete_syntax(Syntax *syntax) {
  delete_value(SYNTAX(syntax));
}

static Syntax *start_pos(Syntax *syntax, Reader *input) {
  if (syntax) {
    if (syntax->file) {
      delete_value(STRING(syntax->file));
    }
    syntax->file = copy_object(input->file_name);
    syntax->start_line = input->line;
    syntax->start_column = input->column;
    syntax->end_line = input->line;
    syntax->end_column = input->column;
  }
  return syntax;
}

static Syntax *end_pos(Syntax *syntax, Reader *input) {
  if (syntax) {
    syntax->end_line = input->line;
    syntax->end_column = input->column;
  }
  return syntax;
}

static Syntax *read_int(Reader *input) {
  int64_t value = 0;
  int sign = 1;
  Syntax *syntax = start_pos(create_syntax(undefined), input);
  if (!syntax) {
    return NULL;
  }
  if (peek(input) == '-') {
    sign = -1;
    pop(input);
  }
  while (isdigit(peek(input))) {
    value = value * 10 + pop(input) - '0';
  }
  if (peek(input) == '.') {
    pop(input);
    double fractional_part = 0;
    double f = 0.1;
    while (isdigit(peek(input))) {
      fractional_part += (pop(input) - '0') * f;
      f /= 10;
    }
    syntax->quoted = F64(sign * (value + fractional_part));
  } else {
    syntax->quoted = I64(sign * value);
  }
  return end_pos(syntax, input);
}

static Syntax *read_string(Reader *input) {
  size_t l = 0;
  size_t size = 10;
  String *buffer = create_string_buffer(size);
  if (!buffer) {
    return NULL;
  }
  pop(input);
  int c = peek(input);
  Syntax *syntax = start_pos(create_syntax(undefined), input);
  if (syntax) {
    int escape = 0;
    while (1) {
      if (c == EOF) {
        raise_error(syntax_error, "unexpected end of file, expected '\"'");
        // TODO: add start pos to error
        delete_value(STRING(buffer));
        delete_syntax(syntax);
        return NULL;
      }
      if (escape) {
        // TODO: more escapes
        switch (c) {
          case 'n':
            c = '\n';
            break;
          case 'r':
            c = '\r';
            break;
          case 't':
            c = '\t';
            break;
          case '0':
            c = '\0';
            break;
        }
        escape = 0;
      } else if (c == '"') {
        pop(input);
        break;
      } else if (c == '\\') {
        escape = 1;
        pop(input);
        c = peek(input);
        continue;
      }
      buffer->bytes[l++] = (uint8_t)c;
      pop(input);
      c = peek(input);
      if (l >= size) {
        size += 10;
        String *new_buffer = resize_string_buffer(buffer, size);
        if (new_buffer) {
          buffer = new_buffer;
        } else {
          raise_error(out_of_memory_error, "out of memory");
          delete_value(STRING(buffer));
          delete_syntax(syntax);
          return NULL;
        }
      }
    }
    buffer->bytes[l] = '\0';
    buffer->length = l;
    syntax->quoted = STRING(buffer);
    if (!RESULT_OK(syntax->quoted)) {
      delete_syntax(syntax);
      syntax = NULL;
    }
  } else {
    delete_value(STRING(buffer));
  }
  return end_pos(syntax, input);
}


static Syntax *read_symbol(Reader *input, SymbolType type) {
  size_t l = 0;
  size_t size = 10;
  String *buffer = create_string_buffer(size);
  if (!buffer) {
    return NULL;
  }
  int c = peek(input);
  int qualified = 0;
  Syntax *syntax = start_pos(create_syntax(undefined), input);
  if (syntax) {
    while (c != EOF && !iswhite(c) && c != '(' && c != ')' && c != '"' && c != ';') {
      if (c == '\\') {
        pop(input);
        c = peek(input);
        if (c == EOF) {
          raise_error(syntax_error, "unexpected end of input");
          delete_value(STRING(buffer));
          delete_syntax(syntax);
          return NULL;
        }
      } else if (c == '/' && l != 0) {
        qualified = 1;
      }
      buffer->bytes[l++] = (uint8_t)c;
      pop(input);
      c = peek(input);
      if (l >= size) {
        size += 10;
        String *new_buffer = resize_string_buffer(buffer, size);
        if (new_buffer) {
          buffer = new_buffer;
        } else {
          raise_error(out_of_memory_error, "out of memory");
          delete_value(STRING(buffer));
          delete_syntax(syntax);
          return NULL;
        }
      }
    }
    buffer->bytes[l] = '\0';
    buffer->length = l;
    if (type == SYMBOL_KEYWORD) {
      syntax->quoted = check_alloc(SYMBOL(intern_keyword(buffer)));
    } else if (type == SYMBOL_UNINTERNED) {
      syntax->quoted = check_alloc(SYMBOL(create_symbol(buffer, NULL)));
    } else if (qualified) {
      syntax->quoted = check_alloc(SYMBOL(find_symbol(buffer)));
      delete_value(STRING(buffer));
    } else {
      syntax->quoted = check_alloc(SYMBOL(module_intern_symbol(input->module, buffer)));
    }
    if (!RESULT_OK(syntax->quoted)) {
      delete_syntax(syntax);
      syntax = NULL;
    }
  } else {
    delete_value(STRING(buffer));
  }
  return end_pos(syntax, input);
}

Syntax *nse_read(Reader *input) {
  char c;
  skip(input);
  c = peek(input);
  if (c == EOF) {
    raise_error(syntax_error, "unexpected end of input");
    return NULL;
  }
  if (c == ';') {
    while (c != '\n' && c != EOF) {
      pop(input);
      c = peek(input);
    }
    return nse_read(input);
  }
  if (c == '.' || c == ')') {
    raise_error(syntax_error, "unexpected '%c'");
    pop(input);
    return NULL;
  }
  if (c == ':') {
    pop(input);
    Syntax *s = read_symbol(input, SYMBOL_KEYWORD);
    if (s) {
      s->quoted.type = VALUE_KEYWORD;
    }
    return s;
  }
  if (c == '\'' || c == '^') {
    Syntax *syntax = start_pos(create_syntax(undefined), input);
    if (syntax) {
      pop(input);
      Value quoted = check_alloc(SYNTAX(nse_read(input)));
      if (RESULT_OK(quoted)) {
        if (c == '^') {
          syntax->quoted = check_alloc(TYPE_QUOTE(create_quote(quoted)));
        } else {
          syntax->quoted = check_alloc(QUOTE(create_quote(quoted)));
        }
        if (RESULT_OK(syntax->quoted)) {
          return end_pos(syntax, input);
        }
      }
      delete_syntax(syntax);
    }
    return NULL;
  }
  if (c == '#') {
    int skip = 0;
    Syntax *syntax = start_pos(create_syntax(undefined), input);
    if (syntax) {
      pop(input);
      c = peek(input);
      if (c == EOF) {
        raise_error(syntax_error, "unexpected end of input");
      } else if (c == ':') {
        pop(input);
        Syntax *s = read_symbol(input, SYMBOL_UNINTERNED);
        if (s) {
          syntax->quoted = s->quoted;
          s->quoted = undefined;
          delete_syntax(s);
          return end_pos(syntax, input);
        }
      } else {
        Symbol *s = module_intern_symbol(input->module, c_string_to_string((char[]){ c, 0 }));
        if (s) {
          Value macro = get_read_macro(s);
          if (RESULT_OK(macro)) {
            delete_value(macro);
            raise_error(syntax_error, "not implemented");
          }
        }
      }
      delete_syntax(syntax);
    }
    if (!skip) {
      return NULL;
    }
    return nse_read(input);
  }
  if (c == '(') {
    Syntax *syntax = start_pos(create_syntax(undefined), input);
    if (syntax) {
      pop(input);
      Vector *vector = read_vector(input, 0);
      if (vector){
        syntax->quoted = VECTOR(vector);
        if (peek(input) == ')') {
          pop(input);
          return end_pos(syntax, input);
        }
        raise_error(syntax_error, "missing ')'");
      }
      delete_syntax(syntax);
    }
    return NULL;
  }
  if (isdigit(c)) {
    return read_int(input);
  }
  if (c == '-' && isdigit(peekn(2, input))) {
    return read_int(input);
  }
  if (c == '"') {
    return read_string(input);
  }
  return read_symbol(input, SYMBOL_INTERNED);
}


static Vector *read_vector(Reader *input, size_t length) {
  skip(input);
  char c = peek(input);
  c = peek(input);
  if (c == EOF || c == ')') {
    return create_vector(length);
  } else {
    Value head = check_alloc(SYNTAX(nse_read(input)));
    if (RESULT_OK(head)) {
      skip(input);
      Vector *vector = read_vector(input, length + 1);
      if (vector) {
        vector->cells[length] = head;
        return vector;
      }
      delete_value(head);
    }
    return NULL;
  }
}
