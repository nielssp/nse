#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "nsert.h"
#include "read.h"

#define MAX_LOOKAHEAD 2

struct reader {
  char type;
  Stream *stream;
  const char *file_name;
  size_t la;
  char la_buffer[MAX_LOOKAHEAD];
  size_t line;
  size_t column;
  Module *module;
};

static Syntax *read_list(Reader *input);

Reader *open_reader(Stream *stream, const char *file_name, Module *module) {
  Reader *s = malloc(sizeof(Reader));
  s->stream = stream;
  s->la = 0;
  s->line = 1;
  s->column = 1;
  s->file_name = file_name;
  s->module = module;
  return s;
}

void close_reader(Reader *reader) {
  stream_close(reader->stream);
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

static Syntax *start_pos(Syntax *syntax, Reader *input) {
  if (syntax) {
    syntax->file = input->file_name;
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
  syntax->quoted = I64(sign * value);
  return end_pos(syntax, input);
}

static Syntax *read_string(Reader *input) {
  size_t l = 0;
  size_t size = 10;
  char *buffer = malloc(size);
  if (!buffer) {
    raise_error("out of memory");
    return NULL;
  }
  pop(input);
  int c = peek(input);
  Syntax *syntax = start_pos(create_syntax(undefined), input);
  if (syntax) {
    int escape = 0;
    while (1) {
      if (c == EOF) {
        raise_error("unexpected end of file, expected '\"'");
        // TODO: add start pos to error
        free(buffer);
        free(syntax);
        return NULL;
      }
      if (escape) {
        // TODO: do something with c
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
      buffer[l++] = (char)c;
      pop(input);
      c = peek(input);
      if (l >= size) {
        size += 10;
        char *new_buffer = realloc(buffer, size);
        if (new_buffer) {
          buffer = new_buffer;
        } else {
          raise_error("out of memory");
          free(buffer);
          free(syntax);
          return NULL;
        }
      }
    }
    syntax->quoted = check_alloc(STRING(create_string(buffer, l)));
    if (!RESULT_OK(syntax->quoted)) {
      free(syntax);
      syntax = NULL;
    }
  }
  free(buffer);
  return end_pos(syntax, input);
}


static Syntax *read_symbol(Reader *input, int keyword) {
  size_t l = 0;
  size_t size = 10;
  char *buffer = malloc(size);
  if (!buffer) {
    raise_error("out of memory");
    return NULL;
  }
  int c = peek(input);
  int qualified = 0;
  Syntax *syntax = start_pos(create_syntax(undefined), input);
  if (syntax) {
    while (c != EOF && !iswhite(c) && c != '(' && c != ')' && c != '"' && c != ';') {
      if (c == '/') {
        qualified = 1;
      }
      buffer[l++] = (char)c;
      pop(input);
      c = peek(input);
      if (l >= size) {
        size += 10;
        char *new_buffer = realloc(buffer, size);
        if (new_buffer) {
          buffer = new_buffer;
        } else {
          raise_error("out of memory");
          free(buffer);
          free(syntax);
          return NULL;
        }
      }
    }
    buffer[l] = '\0';
    if (keyword) {
      syntax->quoted = check_alloc(SYMBOL(intern_keyword(buffer)));
    } else  if (qualified) {
      syntax->quoted = check_alloc(SYMBOL(find_symbol(buffer)));
    } else {
      syntax->quoted = check_alloc(SYMBOL(module_intern_symbol(input->module, buffer)));
    }
    if (!RESULT_OK(syntax->quoted)) {
      free(syntax);
      syntax = NULL;
    }
  }
  free(buffer);
  return end_pos(syntax, input);
}

Syntax *nse_read(Reader *input) {
  char c;
  skip(input);
  c = peek(input);
  if (c == EOF) {
    raise_error("%s:%zu:%zu: end of input", input->file_name, input->line, input->column);
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
    raise_error("%s:%zu:%zu: unexpected '%c'", input->file_name, input->line, input->column, c);
    pop(input);
    return NULL;
  }
  if (c == ':') {
    pop(input);
    Syntax *s = read_symbol(input, 1);
    if (s) {
      s->quoted.type = TYPE_KEYWORD;
    }
    return s;
  }
  if (c == '\'' || c == '^') {
    Syntax *syntax = start_pos(create_syntax(undefined), input);
    if (syntax) {
      pop(input);
      NseVal quoted = check_alloc(SYNTAX(nse_read(input)));
      if (RESULT_OK(quoted)) {
        if (c == '^') {
          syntax->quoted = check_alloc(TQUOTE(create_type_quote(quoted)));
        } else {
          syntax->quoted = check_alloc(QUOTE(create_quote(quoted)));
        }
        del_ref(quoted);
        if (RESULT_OK(syntax->quoted)) {
          return end_pos(syntax, input);
        }
      }
      free(syntax);
    }
    return NULL;
  }
  if (c == '(') {
    Syntax *syntax = start_pos(create_syntax(undefined), input);
    if (syntax) {
      pop(input);
      Syntax *list = read_list(input);
      if (list){
        syntax->quoted = list->quoted;
        free(list);
        if (peek(input) == ')') {
          pop(input);
          return end_pos(syntax, input);
        }
        raise_error("%s:%zu:%zu: missing ')'", input->file_name, input->line, input->column);
        del_ref(syntax->quoted);
      }
      free(syntax);
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
  return read_symbol(input, 0);
}

static Syntax *read_list(Reader *input) {
  Syntax *syntax = start_pos(create_syntax(undefined), input);
  if (!syntax) {
    return NULL;
  }
  char c;
  skip(input);
  c = peek(input);
  if (c == EOF || c == ')') {
    syntax->quoted = nil;
    return end_pos(syntax, input);
  } else {
    NseVal head = check_alloc(SYNTAX(nse_read(input)));
    if (RESULT_OK(head)) {
      NseVal tail;
      skip(input);
      if (peek(input) == '.') {
        pop(input);
        tail = check_alloc(SYNTAX(nse_read(input)));
      } else {
        tail = check_alloc(SYNTAX(read_list(input)));
      }
      if (RESULT_OK(tail)) {
        syntax->quoted = check_alloc(CONS(create_cons(head, tail)));
        del_ref(head);
        del_ref(tail);
        if (RESULT_OK(syntax->quoted)) {
          return end_pos(syntax, input);
        }
        del_ref(tail);
      }
      del_ref(head);
    }
    free(syntax);
    return NULL;
  }
}

