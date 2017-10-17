#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "nsert.h"
#include "read.h"
#include "eval.h"

const char *short_options = "hvlt:Tms:c:";

const struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {"compile", required_argument, NULL, 'c'},
  {0, 0, 0, 0}
};

void describe_option(const char *short_option, const char *long_option, const char *description) {
  printf("  -%-14s --%-18s %s\n", short_option, long_option, description);
}

NseVal sum(NseVal args) {
  int64_t acc = 0;
  while (args.type == TYPE_CONS) {
    acc += head(args).i64;
    args = tail(args);
  }
  return I64(acc);
}

NseVal equals(NseVal args) {
  NseVal previous = undefined;
  while (args.type == TYPE_CONS) {
    NseVal h = head(args);
    if (previous.type != TYPE_UNDEFINED) {
      NseVal result = nse_equals(previous, h);
      if (!is_true(result)) {
        return FALSE;
      }
    }
    previous = h;
    args = tail(args);
  }
  if (!RESULT_OK(previous)) {
    raise_error("too few arguments");
  }
  return TRUE;
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
  Module *system = create_module("system");
  module_define(system, "+", FUNC(sum));
  module_define(system, "=", FUNC(equals));
  Scope *scope = use_module(system);

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
    Stack *stack = open_stack_string(input);
    NseVal code = SYNTAX(parse_prim(stack));
    free(input);
    close_stack(stack);
    NseVal result = eval(code, scope);
    del_ref(code);
    if (result.type == TYPE_UNDEFINED) {
      printf("error: %s: ", error_string);
      if (error_form != NULL) {
        print(error_form->quoted);
      }
    } else {
      print(result);
      del_ref(result);
    }
    printf("\n");
  }
  scope_pop(scope);
  delete_module(system);
  return 0;
}