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
  ARG_POP_ANY(arg, args);
  ARG_DONE(args);
  const char *name = to_string_constant(arg);
  if (name) {
    Stream *f = stream_file(name, "r");
    if (f) {
      Reader *reader = open_reader(f, name, current_scope->module);
      NseVal return_value = nil;
      while (1) {
        Syntax *code = nse_read(reader);
        if (code != NULL) {
          NseVal result = eval(SYNTAX(code), current_scope);
          del_ref(SYNTAX(code));
          if (RESULT_OK(result)) {
            del_ref(result);
          } else {
            return_value = undefined;
            break;
          }
        } else {
          // TODO: check type of error
          clear_error();
          break;
        }
      }
      close_reader(reader);
      return return_value;
    } else {
      raise_error(io_error, "could not open file: %s: %s", name, strerror(errno));
    }
  } else {
    raise_error(domain_error, "must be called with a symbol");
  }
  return undefined;
}

NseVal in_module(NseVal args) {
  ARG_POP_ANY(arg, args);
  ARG_DONE(args);
  const char *name = to_string_constant(arg);
  if (name) {
    Module *m = find_module(name);
    if (m) {
      current_scope->module = m;
      return nil;
    } else {
      raise_error(name_error, "could not find module: %s", name);
    }
  } else {
    raise_error(domain_error, "must be called with a symbol");
  }
  return undefined;
}

NseVal export(NseVal args) {
  ARG_POP_ANY(arg, args);
  ARG_DONE(args);
  const char *name = to_string_constant(arg);
  if (name) {
    return check_alloc(SYMBOL(module_extern_symbol(current_scope->module, name)));
  } else {
    raise_error(domain_error, "must be called with a symbol");
  }
  return undefined;
}

NseVal import(NseVal args) {
  ARG_POP_ANY(arg, args);
  ARG_DONE(args);
  const char *name = to_string_constant(arg);
  if (name) {
    Module *m = find_module(name);
    if (m) {
      import_module(current_scope->module, m);
      return nil;
    } else {
      raise_error(name_error, "could not find module: %s", name);
    }
  } else {
    raise_error(domain_error, "must be called with a symbol");
  }
  return undefined;
}

NseVal intern(NseVal args) {
  ARG_POP_TYPE(Symbol *, symbol, args, to_symbol, "a symbol");
  ARG_DONE(args);
  import_module_symbol(current_scope->module, symbol);
  return nil;
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
  module_ext_define(system_module, "in-module", FUNC(in_module));
  module_ext_define(system_module, "export", FUNC(export));
  module_ext_define(system_module, "import", FUNC(import));
  module_ext_define(system_module, "intern", FUNC(intern));

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

  size_t line = 1;
  char *line_history = NULL;

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
    Reader *reader = open_reader(input_buffer, "(repl)", current_scope->module);
    set_reader_position(reader, line, 1);
    NseVal code = check_alloc(SYNTAX(nse_read(reader)));
    if (RESULT_OK(code)) {
      line = code.syntax->end_line + 1;
      if (line_history) {
        char *new_line_history = string_printf("%s\n%s", line_history, input);
        free(line_history);
        line_history = new_line_history;
      } else {
        line_history = string_printf("%s", input);
      }
      NseVal result = eval(code, current_scope);
      del_ref(code);
      if (RESULT_OK(result)) {
        nse_write(result, stdout_stream, user_module);
        del_ref(result);
      } else {
        printf("error(%s): %s", current_error_type()->name, current_error());
        if (error_form != NULL) {
          NseVal datum = syntax_to_datum(error_form->quoted);
          printf(": ");
          nse_write(datum, stdout_stream, user_module);
          del_ref(datum);
          printf("\nIn %s on line %zd column %zd", error_form->file->chars, error_form->start_line, error_form->start_column);
          if (error_form->start_line > 0) {
            char *line = NULL;
            if (strcmp(error_form->file->chars, "(repl)") == 0) {
              line = get_line(error_form->start_line, line_history);
            } else {
              FILE *f = fopen(error_form->file->chars, "r");
              if (f) {
                line = get_line_in_file(error_form->start_line, f);
                fclose(f);
              }
            }
            if (line) {
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
          printf("\nStack trace:");
          NseVal stack_trace = get_stack_trace();
          for (NseVal it = stack_trace; is_cons(it); it = tail(it)) {
            NseVal syntax = elem(2, head(it));
            printf("\n  %s:%zd:%zd", syntax.syntax->file->chars, syntax.syntax->start_line, syntax.syntax->start_column);
            NseVal datum = syntax_to_datum(syntax.syntax->quoted);
            printf(": ");
            nse_write(datum, stdout_stream, user_module);
            del_ref(datum);
          }
          del_ref(stack_trace);
          clear_error();
          clear_stack_trace();
        }
      }
    } else {
      printf("error: %s", current_error());
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
