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

const char *short_options = "hvlt:Tms:c:";

const struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {"compile", required_argument, NULL, 'c'},
  {0, 0, 0, 0}
};

Scope *current_scope = NULL;

void describe_option(const char *short_option, const char *long_option, const char *description) {
  printf("  -%-14s --%-18s %s\n", short_option, long_option, description);
}

NseVal load(NseVal args) {
  NseVal name = head(args);
  if (RESULT_OK(name)) {
    if (is_symbol(name)) {
      FILE *f = fopen(name.symbol, "r");
      if (f) {
        Stack *stack = open_stack_file(f, name.symbol);
        while (1) {
          Syntax *code = parse_prim(stack);
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
        close_stack(stack);
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

int main(int argc, char *argv[]) {
  int opt;
  int option_index;
  while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
    switch (opt) {
      case 'h':
        printf("usage: %s [options] [lispfile [argument ...]]\n", argv[0]);
        puts("options:");
        describe_option("h", "help", "Show help.");
        describe_option("v", "version", "Show version information.");
        describe_option("c <lispfile>", "compile <lispfile>", "Compile file.");
        return 0;
      case 'v':
        puts("nse-2");
        return 0;
      case 'c':
        // compile: optarg
        puts("not implemented");
        return 1;
    }
  }
  if (optind < argc) {
    // run: argv[optind] w/ args argv[optind + 1], ..., argv[argc - 1]
    puts("not implemented");
    return 1;
  }
  Module *system = get_system_module();
  module_define(system, "load", FUNC(load));

  current_scope = use_module(system);

  rl_bind_key('\t', rl_insert); // TODO: autocomplete
  rl_bind_key('(', paren_start);
  rl_bind_key(')', paren_end);

  while (1) {
    char *input = readline("\001\033[1;32m\002>\001\033[0m\002 ");
    if (input == NULL) {
      // ^D
      printf("\nBye.\n");
      break;
    } else if (input[0] == 0) {
      free(input);
      continue;
    }
    add_history(input);
    Stack *stack = open_stack_string(input, "(user)");
    NseVal code = check_alloc(SYNTAX(parse_prim(stack)));
    free(input);
    close_stack(stack);
    if (RESULT_OK(code)) {
      NseVal result = eval(code, current_scope);
      del_ref(code);
      if (RESULT_OK(result)) {
        nse_write(result, stdout_stream);
        del_ref(result);
      } else {
        printf("error: %s", error_string);
        if (error_form != NULL) {
          NseVal datum = syntax_to_datum(error_form->quoted);
          printf(": ");
          nse_write(datum, stdout_stream);
          del_ref(datum);
          printf("\nIn %s on line %zd column %zd", error_form->file, error_form->start_line, error_form->start_column);
        }
      }
    } else {
      printf("error: %s: ", error_string);
    }
    printf("\n");
  }
  scope_pop(current_scope);
  delete_module(system);
  return 0;
}
