#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "nsert.h"
#include "read.h"
#include "eval.h"

NseVal sum(NseVal args) {
  int64_t acc = 0;
  while (args.type == TYPE_CONS) {
    acc += head(args).i64;
    args = tail(args);
  }
  return I64(acc);
}

int main() {
  Scope *scope = create_scope();
  scope_define(scope, "+", FUNC(sum));
  while (1) {
    char *input = readline("> ");
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
      printf("error: %s\n", error_string);
      if (error_form != NULL) {
        print(error_form->quoted);
      }
    } else {
      print(result);
      del_ref(result);
    }
    printf("\n");
  }
  delete_scope(scope);
  return 0;
}
