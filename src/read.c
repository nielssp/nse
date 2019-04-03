#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "runtime/value.h"
#include "runtime/error.h"
#include "read.h"

#define MAX_LOOKAHEAD 2

struct reader {
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

static Syntax *read_list(Reader *input);

Reader *open_reader(Stream *stream, const char *file_name, Module *module) {
  Reader *s = malloc(sizeof(Reader));
  s->stream = stream;
  s->la = 0;
  s->line = 1;
  s->column = 1;
  s->file_name = create_string(file_name, strlen(file_name));
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
  del_ref(STRING(reader->file_name));
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
  if (syntax->file) {
    del_ref(STRING(syntax->file));
  }
  free(syntax);
}

static Syntax *start_pos(Syntax *syntax, Reader *input) {
  if (syntax) {
    syntax->file = input->file_name;
    add_ref(STRING(input->file_name));
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
  char *buffer = allocate(size);
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
        free(buffer);
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
      buffer[l++] = (char)c;
      pop(input);
      c = peek(input);
      if (l >= size) {
        size += 10;
        char *new_buffer = realloc(buffer, size);
        if (new_buffer) {
          buffer = new_buffer;
        } else {
          raise_error(out_of_memory_error, "out of memory");
          free(buffer);
          delete_syntax(syntax);
          return NULL;
        }
      }
    }
    syntax->quoted = check_alloc(STRING(create_string(buffer, l)));
    if (!RESULT_OK(syntax->quoted)) {
      delete_syntax(syntax);
      syntax = NULL;
    }
  }
  free(buffer);
  return end_pos(syntax, input);
}


static Syntax *read_symbol(Reader *input, SymbolType type) {
  size_t l = 0;
  size_t size = 10;
  char *buffer = allocate(size);
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
          free(buffer);
          delete_syntax(syntax);
          return NULL;
        }
      } else if (c == '/' && l != 0) {
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
          raise_error(out_of_memory_error, "out of memory");
          free(buffer);
          delete_syntax(syntax);
          return NULL;
        }
      }
    }
    buffer[l] = '\0';
    if (type == SYMBOL_KEYWORD) {
      syntax->quoted = check_alloc(SYMBOL(intern_keyword(buffer)));
    } else if (type == SYMBOL_UNINTERNED) {
      syntax->quoted = check_alloc(SYMBOL(create_symbol(buffer, NULL)));
    } else if (qualified) {
      syntax->quoted = check_alloc(SYMBOL(find_symbol(buffer)));
    } else {
      syntax->quoted = check_alloc(SYMBOL(module_intern_symbol(input->module, buffer)));
    }
    if (!RESULT_OK(syntax->quoted)) {
      delete_syntax(syntax);
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
      s->quoted.type = keyword_type;
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
          delete_syntax(s);
          return end_pos(syntax, input);
        }
      } else {
        Symbol *s = module_intern_symbol(input->module, (char[]){ c, 0 });
        if (s) {
          NseVal macro = get_read_macro(s);
          if (RESULT_OK(macro)) {
            syntax->quoted = execute_read(input, macro, &skip);
            if (RESULT_OK(syntax->quoted)) {
              return end_pos(syntax, input);
            }
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
      Syntax *list = read_list(input);
      if (list){
        syntax->quoted = list->quoted;
        delete_syntax(list);
        if (peek(input) == ')') {
          pop(input);
          return end_pos(syntax, input);
        }
        raise_error(syntax_error, "missing ')'");
        del_ref(syntax->quoted);
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
    delete_syntax(syntax);
    return NULL;
  }
}

NseVal execute_read(Reader *reader, NseVal read, int *skip) {
  Symbol *action = to_symbol(read);
  if (action) {
    if (action == read_char_symbol) {
      int c = peek(reader);
      if (c != EOF) {
        pop(reader);
        return I64(c);
      } else {
        raise_error(syntax_error, "unexpected end of input");
        return undefined;
      }
    } else if (action == read_string_symbol) {
      return check_alloc(SYNTAX(read_string(reader)));
    } else if (action == read_symbol_symbol) {
      return check_alloc(SYNTAX(read_symbol(reader, SYMBOL_INTERNED)));
    } else if (action == read_int_symbol) {
      return check_alloc(SYNTAX(read_int(reader)));
    } else if (action == read_any_symbol) {
      return check_alloc(SYNTAX(nse_read(reader)));
    } else if (action == read_ignore_symbol) {
      *skip = 1;
      return undefined;
    } else {
      raise_error(domain_error, "invalid read action");
    }
  } else if (is_cons(read)) {
    action = to_symbol(head(read));
    if (action) {
      if (action == read_bind_symbol) {
        NseVal result = undefined;
        NseVal action_a = elem(1, read);
        NseVal transform = THEN(action_a, elem(2, read));
        if (RESULT_OK(transform)) {
          NseVal value = execute_read(reader, action_a, skip);
          if (RESULT_OK(value)) {
            NseVal transform_args = check_alloc(CONS(create_cons(value, nil)));
            if (RESULT_OK(transform_args)) {
              NseVal transform_result = nse_apply(transform, transform_args);
              if (RESULT_OK(transform_result)) {
                result = execute_read(reader, transform_result, skip);
                del_ref(transform_result);
              }
              del_ref(transform_args);
            }
            del_ref(value);
          }
        }
        return result;
      } else if (action == read_return_symbol) {
        return add_ref(elem(1, read));
      } else {
        raise_error(domain_error, "invalid read action");
      }
    } else {
      raise_error(domain_error, "invalid read action");
    }
  } else {
    raise_error(domain_error, "invalid read action");
  }
  return undefined;
}
