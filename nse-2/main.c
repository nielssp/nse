#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "nsert.h"
#include "read.h"
#include "write.h"
#include "eval.h"
#include "system.h"

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

NseVal load(NseVal args) {
  NseVal name = head(args);
  if (RESULT_OK(name)) {
    if (is_symbol(name)) {
      Stream *f = stream_file(name.symbol->name, "r");
      if (f) {
        Reader *reader = open_reader(f, name.symbol->name, current_scope->module);
        while (1) {
          Syntax *code = nse_read(reader);
          if (code != NULL) {
            NseVal result = eval(SYNTAX(code), current_scope);
            del_ref(SYNTAX(code));
            if (RESULT_OK(result)) {
              del_ref(result);
            } else {
              name = undefined;
              break;
            }
          } else {
            // TODO: check type of error
            clear_error();
            break;
          }
        }
        close_reader(reader);
        return name;
      } else {
        raise_error("could not open file: %s: %s", name.symbol, strerror(errno));
      }
    } else {
      raise_error("must be called with a symbol");
    }
  }
  return undefined;
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
  while (*(text++)) {
    if (line == lines) {
      length++;
      if (*text == '\n') {
        break;
      }
    } else if (*text == '\n') {
      lines++;
    }
  }
  char *line_buf = malloc(length + 1);
  line_buf[length] = 0;
  if (length == 0) {
    return line_buf;
  }
  memcpy(line_buf, text - length - 1, length);
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
        puts("nse-2");
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
  system_module = get_system_module();
  module_ext_define(system_module, "load", FUNC(load));

  Module *user_module = create_module("user");
  import_module(user_module, lang_module);
  import_module(user_module, system_module);

  current_scope = use_module(user_module);

  rl_bind_key('\t', rl_complete);
  rl_bind_key('(', paren_start);
  rl_bind_key(')', paren_end);

  rl_attempted_completion_function = symbol_completion;

  if (std) {
    NseVal args = CONS(create_cons(SYMBOL(intern_keyword("std.lisp")), nil));
    del_ref(load(args));
    del_ref(args);
  }

  while (1) {
    char *prompt = string_printf("\001\033[1;32m\002%s>\001\033[0m\002 ", module_name(current_scope->module));
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
    Stream *input_buffer = stream_buffer(input, strlen(input));
    Reader *reader = open_reader(input_buffer, "(user)", user_module);
    NseVal code = check_alloc(SYNTAX(nse_read(reader)));
    if (RESULT_OK(code)) {
      NseVal result = eval(code, current_scope);
      del_ref(code);
      if (RESULT_OK(result)) {
        nse_write(result, stdout_stream, user_module);
        del_ref(result);
      } else {
        printf("error: %s", current_error());
        if (error_form != NULL) {
          NseVal datum = syntax_to_datum(error_form->quoted);
          printf(": ");
          nse_write(datum, stdout_stream, user_module);
          del_ref(datum);
          printf("\nIn %s on line %zd column %zd", error_form->file, error_form->start_line, error_form->start_column);
          if (error_form->start_line > 0) {
            char *line = get_line(error_form->start_line, input);
            printf("\n%s\n", line);
            for (size_t i = 1; i < error_form->start_column; i++) {
              printf(" ");
            }
            printf("^");
            if (error_form->start_line == error_form->end_line) {
              size_t length = error_form->end_column - error_form->start_column - 1;
              for (size_t i = 0; i < length; i++) {
                printf("^");
              }
            }
            free(line);
          }
        }
      }
    } else {
      printf("error: %s: ", current_error());
    }
    close_reader(reader);
    free(input);
    printf("\n");
  }
  scope_pop(current_scope);
  return 0;
}
