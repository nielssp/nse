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

const char *short_options = "hvc:n";

const struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {"compile", required_argument, NULL, 'c'},
  {"no-std", no_argument, NULL, 'n'},
  {0, 0, 0, 0}
};

Module *system_module = NULL;
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


void print_error_line(char *line_history, String *file_name, size_t start_line, size_t start_column, size_t end_line, size_t end_column) {
  printf("\nIn %s on line %zd column %zd", TO_C_STRING(file_name), start_line, start_column);
  if (start_line > 0) {
    char *line = NULL;
    if (strcmp(TO_C_STRING(file_name), "(repl)") == 0) {
      line = get_line(start_line, line_history);
    } else {
      FILE *f = fopen(TO_C_STRING(file_name), "r");
      if (f) {
        line = get_line_in_file(start_line, f);
        fclose(f);
      }
    }
    if (line) {
      printf("\n%s\n", line);
      for (size_t i = 1; i < start_column; i++) {
        printf(" ");
      }
      printf("^");
      if (start_line == end_line && end_column > start_column) {
        size_t length = end_column - start_column - 1;
        for (size_t i = 0; i < length; i++) {
          printf("^");
        }
      }
      free(line);
    }
  }
}


int main(int argc, char *argv[]) {
  int opt;
  int option_index;
  int std = 1;
  while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
    switch (opt) {
      case 'h':
        printf("usage: %s [options] [lispfile [argument ...]]\n", argv[0]);
        puts("options:");
        describe_option("h", "help", "Show help.");
        describe_option("v", "version", "Show version information.");
        describe_option("c <lispfile>", "compile <lispfile>", "Compile file.");
        describe_option("n", "no-std", "Don't load standard library");
        return 0;
      case 'v':
        puts("nse-3");
        return 0;
      case 'c':
        // compile: optarg
        puts("not implemented");
        return 1;
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
  }


  current_scope = use_module(user_module);

  rl_bind_key('\t', rl_complete);
  rl_bind_key('(', paren_start);
  rl_bind_key(')', paren_end);

  rl_attempted_completion_function = symbol_completion;

  size_t line = 1;
  char *line_history = NULL;

  while (1) {
    char *prompt = string_printf("\001\033[1;32m\002%s>\001\033[0m\002 ", TO_C_STRING(get_module_name(current_scope->module)));
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
    size_t input_length = strlen(input);
    Stream *input_buffer = stream_buffer(input, input_length, input_length);
    Reader *reader = open_reader(input_buffer, "(repl)", current_scope->module);
    set_reader_position(reader, line, 1);
    Value code = check_alloc(SYNTAX(nse_read(reader)));
    int error = 0;
    if (RESULT_OK(code)) {
      nse_write(code, stdout_stream, user_module);
      delete_value(code);
    } else {
      error = 1;
    }
    get_reader_position(reader, NULL, &line, NULL);
    line += 1;
    if (line_history) {
      char *new_line_history = string_printf("%s\n%s", line_history, input);
      free(line_history);
      line_history = new_line_history;
    } else {
      line_history = string_printf("%s", input);
    }
    if (error) {
      printf("error(%s): %s", TO_C_STRING(current_error_type()->name), current_error());
      if (!RESULT_OK(code)) {
        String *file_name;
        size_t current_line, current_column;
        get_reader_position(reader, &file_name, &current_line, &current_column);
        print_error_line(line_history, file_name, current_line, current_column, current_line, current_column);
      }
      clear_error();
    }
    close_reader(reader);
    free(input);
    printf("\n");
  }
  if (line_history) {
    free(line_history);
  }
  scope_pop(current_scope);
  return 0;
}
