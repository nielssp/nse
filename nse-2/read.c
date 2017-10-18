#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "nsert.h"
#include "read.h"

#define MAX_LOOKAHEAD 2

#define STACK_FILE 1
#define STACK_STRING 2

struct stack {
  char type;
  union {
    FILE *file;
    const char *string;
  };
  const char *file_name;
  size_t la;
  char la_buffer[MAX_LOOKAHEAD];
  size_t line;
  size_t column;
};

Syntax *parse_list(Stack *input);

Stack *open_stack_file(FILE *file, const char *file_name) {
  Stack *s = malloc(sizeof(Stack));
  s->type = STACK_FILE;
  s->file = file;
  s->la = 0;
  s->line = 1;
  s->column = 1;
  s->file_name = file_name;
  return s;
}

Stack *open_stack_string(const char *string, const char *file_name) {
  Stack *s = malloc(sizeof(Stack));
  s->type = STACK_STRING;
  s->string = string;
  s->la = 0;
  s->line = 1;
  s->column = 1;
  s->file_name = file_name;
  return s;
}

void close_stack(Stack *s) {
  if (s->type == STACK_FILE) {
    fclose(s->file);
  }
  free(s);
}

int pop(Stack *s) {
  int c;
  if (s->la > 0) {
    c = s->la_buffer[0];
    s->la--;
    for (int i =0; i < s->la; i++) {
      s->la_buffer[i] = s->la_buffer[i + 1];
    }
  } else {
    if (s->type == STACK_FILE) {
      c = fgetc(s->file);
    } else {
      if (s->string[0] == 0) {
        c = EOF;
      } else {
        c = s->string[0];
        s->string++;
      }
    }
  }
  if (c == '\n') {
    s->line++;
    s->column = 1;
  } else {
    s->column++;
  }
  return c;
}

int peekn(size_t n, Stack *s) {
  while (s->la < n) {
    int c;
    if (s->type == STACK_FILE) {
      c = fgetc(s->file);
      if (c == EOF) {
        return EOF;
      }
    } else {
      if (s->string[0] == 0) {
        return EOF;
      } else {
        c = s->string[0];
        s->string++;
      }
    }
    s->la_buffer[s->la] = (char)c;
    s->la++;
  }
  return s->la_buffer[n - 1];
}

char peek(Stack *s) {
  return peekn(1, s);
}

int iswhite(int c) {
  return c == '\n' || c == '\r' || c == '\t' || c == ' ';
}

void skip(Stack *input) {
  while (iswhite(peek(input))) {
    pop(input);
  }
}

Syntax *start_pos(Syntax *syntax, Stack *input) {
  if (syntax) {
    syntax->file = input->file_name;
    syntax->start_line = input->line;
    syntax->start_column = input->column;
    syntax->end_line = input->line;
    syntax->end_column = input->column;
  }
  return syntax;
}

Syntax *end_pos(Syntax *syntax, Stack *input) {
  if (syntax) {
    syntax->end_line = input->line;
    syntax->end_column = input->column;
  }
  return syntax;
}

Syntax *parse_int(Stack *input) {
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

Syntax *parse_symbol(Stack *input) {
  size_t l = 0;
  size_t size = 10;
  char *buffer = malloc(size);
  if (!buffer) {
    raise_error("out of memory");
    return NULL;
  }
  int c = peek(input);
  Syntax *syntax = start_pos(create_syntax(undefined), input);
  if (syntax) {
    while (c != EOF && !iswhite(c) && c != '(' && c != ')') {
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
          break;
        }
      }
    }
    buffer[l] = '\0';
    syntax->quoted = check_alloc(SYMBOL(create_symbol(buffer)));
    if (!RESULT_OK(syntax->quoted)) {
      free(syntax);
      syntax = NULL;
    }
  }
  free(buffer);
  return end_pos(syntax, input);
}

Syntax *parse_prim(Stack *input) {
  char c;
  skip(input);
  c = peek(input);
  if (c == EOF) {
    raise_error("%s:%zu:%zu: end of input", input->file_name, input->line, input->column);
    return NULL;
  }
  if (c == '.') {
    raise_error("%s:%zu:%zu: unexpected '.'", input->file_name, input->line, input->column);
    pop(input);
    return NULL;
  }
  if (c == '\'') {
    Syntax *syntax = start_pos(create_syntax(undefined), input);
    if (syntax) {
      pop(input);
      NseVal quoted = SYNTAX(parse_prim(input));
      if (RESULT_OK(quoted)) {
        syntax->quoted = check_alloc(QUOTE(create_quote(quoted)));
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
      Syntax *list = parse_list(input);
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
    return parse_int(input);
  }
  if (c == '-' && isdigit(peekn(2, input))) {
    return parse_int(input);
  }
  return parse_symbol(input);
}

Syntax *parse_list(Stack *input) {
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
    NseVal head = SYNTAX(parse_prim(input));
    if (RESULT_OK(head)) {
      NseVal tail;
      skip(input);
      if (peek(input) == '.') {
        pop(input);
        tail = SYNTAX(parse_prim(input));
      } else {
        tail = SYNTAX(parse_list(input));
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

