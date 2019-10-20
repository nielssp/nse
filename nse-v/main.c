/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "value.h"
#include "type.h"
#include "error.h"
#include "lang.h"
#include "module.h"
#include "read.h"
#include "write.h"
#include "eval.h"
#include "system.h"

#define SGR_RESET "\001\033[0m\002"

#define SGR_BOLD_GREEN "\001\033[1;32m\002"
#define SGR_RED "\001\033[31m\002"
#define SGR_BOLD "\001\033[1m\002"

const char *short_options = "hvc:ne:p:";

const struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {"compile", required_argument, NULL, 'c'},
  {"no-std", no_argument, NULL, 'n'},
  {"eval", required_argument, NULL, 'e'},
  {"print", required_argument, NULL, 'p'},
  {0, 0, 0, 0}
};

Scope *current_scope = NULL;

void describe_option(const char *short_option, const char *long_option, const char *description) {
  printf("  -%-14s --%-18s %s\n", short_option, long_option, description);
}

int paren_start(int count, int key) {
  rl_insert_text("()");
  rl_point--;
  return 0;
}

int paren_end(int count, int key) {
  if (rl_point < rl_end && rl_line_buffer[rl_point] == ')') {
    rl_point++;
  } else {
    rl_insert_text(")");
  }
  return 0;
}

char *get_line(size_t line, const char *text) {
  size_t lines = 1;
  size_t length = 0;
  for (; *text; text++) {
    if (line == lines) {
      if (*text == '\n') {
        break;
      }
      length++;
    } else if (*text == '\n') {
      lines++;
    }
  }
  char *line_buf = malloc(length + 1);
  line_buf[length] = 0;
  if (length == 0) {
    return line_buf;
  }
  memcpy(line_buf, text - length, length);
  return line_buf;
}

char *get_line_in_file(size_t line, FILE *f) {
  size_t lines = 1;
  size_t length = 0;
  long offset = 0;
  int c = getc(f);
  while (c != EOF) {
    if (line == lines) {
      if (c == '\n') {
        break;
      }
      length++;
    } else if (c == '\n') {
      lines++;
      offset = ftell(f);
    }
    c = getc(f);
  }
  char *line_buf = malloc(length + 1);
  if (length == 0) {
    line_buf[0] = 0;
    return line_buf;
  }
  fseek(f, offset, SEEK_SET);
  size_t r = fread(line_buf, 1, length, f);
  line_buf[r] = 0;
  return line_buf;
}

char **symbols = NULL;

char *symbol_generator(const char *text, int state) {
  static int i, length;
  static const char *ptr;
  if (!state) {
    i = 0;
    length = strlen(text);
    int offset;
    for (offset = length - 1; offset >= 0; offset--) {
      char c = text[offset];
      if (iswhite(c) || c == '(' || c == ')' || c == '\'' || c == '"') {
        break;
      }
      if (c == ';') {
        // comment
        break;
      }
      if (c == '^') {
        // type
        break;
      }
    }
    ptr = text + (offset + 1);
    length -= offset + 1;
    if (symbols) {
      for (char **entry = symbols; *entry; entry++) {
        free(*entry);
      }
      free(symbols);
    }
    symbols = get_symbols(current_scope->module);
  }
  char *match;
  while (1) {
    match = symbols[i++];
    if (!match) {
      break;
    } else if (strncmp(match, ptr, length) == 0) {
      return string_printf(match);
    }
  }
  return NULL;
}

char **symbol_completion(const char *text, int start, int end) {
  rl_attempted_completion_over = 1;
  return rl_completion_matches(text, symbol_generator);
}


void print_error_line(char *line_history, String *file_name, size_t start_line, size_t start_column, size_t end_line, size_t end_column, Stream *stream) {
  if (start_line > 0) {
    char *line = NULL;
    if (strcmp(TO_C_STRING(file_name), "(repl)") == 0 || strcmp(TO_C_STRING(file_name), "(cli)") == 0) {
      line = get_line(start_line, line_history);
    } else {
      FILE *f = fopen(TO_C_STRING(file_name), "r");
      if (f) {
        line = get_line_in_file(start_line, f);
        fclose(f);
      }
    }
    if (line) {
      stream_printf(stream, "\n%s\n", line);
      for (size_t i = 1; i < start_column; i++) {
        stream_printf(stream, " ");
      }
      stream_printf(stream, SGR_RED);
      stream_printf(stream, "^");
      if (start_line == end_line && end_column > start_column) {
        size_t length = end_column - start_column - 1;
        for (size_t i = 0; i < length; i++) {
          stream_printf(stream, "~");
        }
      }
      stream_printf(stream, SGR_RESET);
      free(line);
    }
  }
}

Value read_and_eval(char *expr, const char *filename, Module *module, char **line_history, size_t *line, Stream *error_stream) {
  Value result = undefined;
  size_t input_length = strlen(expr);
  Stream *input_buffer = stream_buffer(expr, input_length, input_length);
  Reader *reader = open_reader(input_buffer, filename, current_scope->module);
  set_reader_position(reader, *line, 1);
  Value code = check_alloc(SYNTAX(nse_read(reader)));
  int error = 0;
  if (RESULT_OK(code)) {
    result = eval(copy_value(code), current_scope);
    delete_value(code);
    if (!RESULT_OK(result)) {
      error = 1;
    }
  } else {
    error = 1;
  }
  get_reader_position(reader, NULL, line, NULL);
  *line += 1;
  if (*line_history) {
    char *new_line_history = string_printf("%s\n%s", *line_history, expr);
    free(*line_history);
    *line_history = new_line_history;
  } else {
    *line_history = string_printf("%s", expr);
  }
  if (error) {
    String *file_name = NULL;
    size_t start_line, start_column, end_line, end_column;
    if (!RESULT_OK(code)) {
      get_reader_position(reader, &file_name, &start_line, &start_column);
      end_line = start_line;
      end_column = start_column;
    } else if (error_form) {
      file_name = error_form->file;
      start_line = error_form->start_line;
      start_column = error_form->start_column;
      end_line = error_form->end_line;
      end_column = error_form->end_column;
    }
    if (file_name) {
      stream_printf(error_stream, SGR_BOLD "%s:%zd:%zd: ", TO_C_STRING(file_name), start_line, start_column);
    }
    if (current_error()) {
      stream_printf(error_stream, SGR_RED "error(%s):" SGR_RESET SGR_BOLD " %s" SGR_RESET,
          TO_C_STRING(current_error_type()->name), current_error());
    } else {
      stream_printf(error_stream, SGR_RED "error:" SGR_RESET SGR_BOLD " unspecified error" SGR_RESET);
    }
    if (!RESULT_OK(code)) {
      print_error_line(*line_history, file_name, start_line, start_column, end_line, end_column, error_stream);
    } else if (error_form) {
      Value datum = syntax_to_datum(copy_value(error_form->quoted));
      stream_printf(error_stream, ": ");
      nse_write(datum, error_stream, module, 20);
      delete_value(datum);
      print_error_line(*line_history, file_name, start_line, start_column, end_line, end_column, error_stream);
    }
    List *trace = get_stack_trace();
    if (trace) {
      stream_printf(error_stream, "\nStack trace:");
      for (; trace; trace = trace->tail) {
        Syntax *syntax = TO_SYNTAX(TO_VECTOR(trace->head)->cells[2]);
        stream_printf(error_stream, "\n  %s:%zd:%zd", TO_C_STRING(syntax->file),
            syntax->start_line, syntax->start_column);
        Value datum = syntax_to_datum(copy_value(syntax->quoted));
        stream_printf(error_stream, ": ");
        nse_write(datum, error_stream, module, 20);
        delete_value(datum);
      }
    }
    clear_error();
    clear_stack_trace();
  }
  close_reader(reader);
  return result;
}

int main(int argc, char *argv[]) {
  int opt;
  int option_index;
  int std = 1;
  int eval = 0;
  while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
    switch (opt) {
      case 'h':
        printf("usage: %s [options] [lispfile [argument ...]]\n", argv[0]);
        puts("options:");
        describe_option("h", "help", "Show help.");
        describe_option("v", "version", "Show version information.");
        describe_option("c <lispfile>", "compile <lispfile>", "Compile file.");
        describe_option("e <expr>", "eval <expr>", "Evaluate expression.");
        describe_option("p <expr>", "print <expr>", "Evaluate expression and print result.");
        describe_option("n", "no-std", "Don't load standard library");
        return 0;
      case 'v':
        puts("nse-3");
        return 0;
      case 'c':
        // compile: optarg
        puts("not implemented");
        return 1;
      case 'e':
      case 'p':
        eval = 1;
        break;
      case 'n':
        std = 0;
        break;
    }
  }
  if (optind < argc) {
    // run: argv[optind] w/ args argv[optind + 1], ..., argv[argc - 1]
    puts("not implemented");
    return 1;
  }

  Module *user_module = create_module("user");
  if (std) {
    import_module(user_module, lang_module);
    import_module(user_module, get_system_module());
  }

  size_t line = 1;
  char *line_history = NULL;

  current_scope = use_module(user_module);

  if (eval) {
    Value result = undefined;
    optind = 1;
    int ok = 1;
    while (ok && (opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
      switch (opt) {
        case 'e':
        case 'p': {
          if (RESULT_OK(result)) {
            delete_value(result);
          }
          result = read_and_eval(optarg, "(cli)", user_module, &line_history, &line, stderr_stream);
          if (!RESULT_OK(result)) {
            puts("");
            ok = 0;
          }
          if (opt == 'p') {
            nse_write(result, stdout_stream, user_module, 20);
            puts("");
          }
          break;
        }
      }
    }
    if (RESULT_OK(result)) {
      delete_value(result);
    }
    if (line_history) {
      free(line_history);
    }
    scope_pop(current_scope);
    return RESULT_OK(result) ? 0 : 1;
  }

  rl_bind_key('\t', rl_complete);
  rl_bind_key('(', paren_start);
  rl_bind_key(')', paren_end);

  rl_attempted_completion_function = symbol_completion;

  while (1) {
    char *prompt = string_printf(SGR_BOLD_GREEN "%s>" SGR_RESET " ", TO_C_STRING(get_module_name(current_scope->module)));
    char *input = readline(prompt);
    free(prompt);
    if (input == NULL) {
      // ^D
      printf("\nBye.\n");
      break;
    } else if (input[0] == 0) {
      free(input);
      continue;
    }
    add_history(input);
    Value result = read_and_eval(input, "(repl)", user_module, &line_history, &line, stdout_stream);
    if (RESULT_OK(result)) {
      nse_write(result, stdout_stream, user_module, 20);
      delete_value(result);
    }
    free(input);
    printf("\n");
  }
  if (line_history) {
    free(line_history);
  }
  scope_pop(current_scope);
  return 0;
}
