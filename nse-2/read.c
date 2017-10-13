
#include "nsert.h"

#define MAX_LOOKAHEAD 2

#define STACK_FILE 1
#define STACK_STRING 2

typedef struct stack {
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
} Stack;

Stack *open_stack_file(FILE *file) {
  Stack *s = malloc(sizeof(Stack));
  s->type = STACK_FILE;
  s->file = file;
  s->la = 0;
  s->line = 1;
  s->column = 1;
  s->file_name = NULL;
  return s;
}

Stack *open_stack_string(const char *string) {
  Stack *s = malloc(sizeof(Stack));
  s->type = STACK_STRING;
  s->string = string;
  s->la = 0;
  s->line = 1;
  s->column = 1;
  s->file_name = NULL;
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
  // TODO: should be peek position
  syntax->start_line = input->line;
  syntax->start_column = input->column;
  syntax->end_line = input->line;
  syntax->end_column = input->column;
  return syntax;
}

Syntax *end_pos(Syntax *syntax, Stack *input) {
  syntax->end_line = input->line;
  syntax->end_column = input->column;
  return syntax;
}

NseVal parse_int(Stack *input) {
  int value = 0;
  int sign = 1;
  Syntax *syntax = start_pos(create_syntax(undefined));
  if (peek(input) == '-') {
    sign = -1;
    pop(input);
  }
  while (isdigit(peek(input))) {
    value = value * 10 + pop(input) - '0';
  }
  syntax->quoted = INT(sign * value);
  end_pos(synta);
  return syntax;
}
