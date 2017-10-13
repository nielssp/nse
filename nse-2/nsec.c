#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "nsert.h"

typedef struct env Env;
struct env {
  char *name;
  Env *next;
};
Env *env_push(Env *env, char *name) {
  Env *new = malloc(sizeof(Env));
  new->next = env;
  new->name = name;
  return new;
}
Env *env_push_params(Env *env, NseVal params) {
  while (params.type == TYPE_CONS) {
    env = env_push(env, head(params).value.sval);
    params = tail(params);
  }
  if (params.type == TYPE_SYMBOL) {
    env = env_push(env, params.value.sval);
  }
  return env;
}
void env_delete_until(Env *start, Env *end) {
  while (start != end) {
    Env *next = start->next;
    free(start);
    start = next;
  }
}

int has_main = 0;


NseVal parse_symbol(Stack *input) {
  NseVal prim;
  size_t l = 0;
  size_t size = 10;
  char *buffer = malloc(size);
  int c = peek(input);
  while (c != EOF && !iswhite(c) && c != '(' && c != ')') {
    buffer[l++] = (char)c;
    pop(input);
    c = peek(input);
    if (l >= size) {
      size += 10;
      buffer = realloc(buffer, size);
    }
  }
  buffer[l] = '\0';
  prim = Symbol(buffer);
  free(buffer);
  return prim;
}

NseVal parse_list(Stack *input);

NseVal parse_prim(Stack *input) {
  char c;
  skip(input);
  c = peek(input);
  if (c == EOF) {
    printf("error: %s:%zu:%zu: end of input\n", input->file_name, input->line, input->column);
    return Nil;
  }
  if (c == '.') {
    pop(input);
    printf("error: %s:%zu:%zu: unexpected '.'\n", input->file_name, input->line, input->column);
    return Nil;
  }
  if (c == '\'') {
    pop(input);
    return Quote(parse_prim(input));
  }
  if (c == '(') {
    pop(input);
    NseVal list = parse_list(input);
    if (peek(input) != ')') {
      printf("error: %s:%zu:%zu: missing ')'\n", input->file_name, input->line, input->column);
    } else {
      pop(input);
    }
    return list;
  }
  if (isdigit(c)) {
    return parse_int(input);
  }
  if (c == '-' && isdigit(peekn(2, input))) {
    return parse_int(input);
  }
  return parse_symbol(input);
}

NseVal parse_list(Stack *input) {
  char c;
  skip(input);
  c = peek(input);
  if (c == EOF || c == ')') {
    return Nil;
  }
  NseVal head = parse_prim(input);
  skip(input);
  if (peek(input) == '.') {
    pop(input);
    return Cons(head, parse_prim(input));
  } else {
    return Cons(head, parse_list(input));
  }
}

char *sappend(const char *s1, const char *s2) {
  size_t l1 = strlen(s1), l2 = strlen(s2);
  char *res = malloc(l1 + l2 + 1);
  memcpy(res, s1, l1);
  memcpy(res + l1, s2, l2);
  res[l1 + l2] = '\0';
  return res;
}

int is_list(NseVal p) {
  return p.type == TYPE_CONS || p.type == TYPE_NIL;
}

int is_macro(NseVal p, const char *name) {
  if (p.type != TYPE_CONS) {
    return 0;
  }
  return is_symbol(head(p), name);
}

void write_symbol(FILE *f, const char *s) {
  if (strcmp("main", s) == 0) {
    fprintf(f, "__main");
    has_main = 1;
  } else {
    while (*s) {
      if (isdigit(*s) || isalpha(*s)) {
        fputc(*s, f);
      } else {
        fprintf(f, "_%02x", (unsigned char)*s);
      }
      s++;
    }
  }
}

void write_uppercase_symbol(FILE *f, const char *s) {
  while (*s) {
    if (isdigit(*s) || isalpha(*s)) {
      fputc(toupper(*s), f);
    } else {
      fprintf(f, "_%02X", (unsigned char)*s);
    }
    s++;
  }
}

void write_decl(NseVal def, FILE *cf, FILE *hf, int public) {
  if (is_macro(def, "define")) {
    FILE *f = public ? hf : cf;
    NseVal name = head(tail(def));
    fprintf(f, "/* define %s */\n", name.value.sval);
    if (tail(tail(tail(def))).type == TYPE_CONS) {
      fprintf(f, "NseVal ");
      write_symbol(f, name.value.sval);
      fprintf(f, "(NseVal);\n");
    }
    fprintf(cf, "NseVal __var_");
    write_symbol(cf, name.value.sval);
    fprintf(cf, ";\n");
    if (public) {
      fprintf(hf, "extern NseVal __var_");
      write_symbol(hf, name.value.sval);
      fprintf(hf, ";\n");
    }
  } else if (is_macro(def, "public")) {
    NseVal next = tail(def);
    while (next.type == TYPE_CONS) {
      write_decl(head(next), cf, hf, 1);
      next = tail(next);
    }
  } else if (is_macro(def, "private")) {
    NseVal next = tail(def);
    while (next.type == TYPE_CONS) {
      write_decl(head(next), cf, hf, 0);
      next = tail(next);
    }
  } else if (is_macro(def, "import")) {
    NseVal name = head(tail(def));
    fprintf(cf, "#include \"%s.h\"\n", name.value.sval);
  } else {
    printf("warning: not a valid definition:\n");
    nse_print(def);
    printf("\n");
  }
}

void write_decls(NseVal module, FILE *cf, FILE *hf) {
  if (!is_macro(module, "module")) {
    printf("warning: root-expression should be a module\n");
    return;
  }
  NseVal next = tail(module);
  while (next.type == TYPE_CONS) {
    write_decl(head(next), cf, hf, 1);
    next = tail(next);
  }
}

int write_lambda_decls(NseVal prim, FILE *cf, int counter) {
  int i;
  if (is_macro(prim, "lambda")) {
    fprintf(cf, "static NseVal __anon_%d(NseVal, NseVal env[]);\n", counter);
    return write_lambda_decls(tail(tail(prim)), cf, ++counter);
  }
  switch (prim.type) {
    case TYPE_CONS:
      counter = write_lambda_decls(head(prim), cf, counter);
      return write_lambda_decls(tail(prim), cf, counter);
    case TYPE_ARRAY:
      for (i = 0; i < prim.value.aval->size; i++) {
        counter = write_lambda_decls(prim.value.aval->array[i], cf, counter);
      }
      return counter;
    default:
      return counter;
  }
}

void write_quote(NseVal prim, FILE *cf) {
  switch (prim.type) {
    case TYPE_NIL:
      fprintf(cf, "Nil");
      break;
    case TYPE_CONS:
      fprintf(cf, "QCons(");
      write_quote(prim.value.cval->h, cf);
      fprintf(cf, ", ");
      write_quote(prim.value.cval->t, cf);
      fprintf(cf, ")");
      break;
    case TYPE_SYMBOL:
      fprintf(cf, "Symbol(\"");
      char *c = prim.value.sval;
      while (*c) {
        switch (*c) {
          case '\n':
            fprintf(cf, "\\n");
            break;
          case '"':
          case '\\':
            fprintf(cf, "\\%c", *c);
            break;
          default:
            fprintf(cf, "%c", *c);
        }
        c++;
      }
      fprintf(cf, "\")");
      break;
    case TYPE_INT:
      fprintf(cf, "Int(%d)", prim.value.ival);
      break;
    case TYPE_QUOTE:
      fprintf(cf, "QQuote(");
      write_quote(prim.value.qval->quoted, cf);
      fprintf(cf, ")");
      break;
  }
}

int write_expr(NseVal expr, FILE *cf, Env *env, int counter) {
  if (is_macro(expr, "'anon")) {
    int i = head(tail(expr)).value.ival;
    fprintf(cf, "Closure(__anon_%d, ", i);
    int size = 0;
    if (env == NULL) {
      fprintf(cf, "NULL, 0)");
    } else {
      fprintf(cf, "(NseVal[]){");
      for (Env *var = env; var != NULL; var = var->next, size++) {
        if (size > 0) {
          fprintf(cf, ", ");
        }
        fprintf(cf, "__var_");
        write_symbol(cf, var->name);
      }
      fprintf(cf, "}, %d)", size);
    }
    return counter;
  }
  if (is_macro(expr, "c-int")) {
    fprintf(cf, "(");
    counter = write_expr(head(tail(expr)), cf, env, counter);
    fprintf(cf, ").value.ival");
    return counter;
  }
  if (is_macro(expr, "c-op")) {
    fprintf(cf, "((");
    counter = write_expr(head(tail(expr)), cf, env, counter);
    fprintf(cf, ") %s (", head(tail(tail(expr))).value.sval);
    counter = write_expr(head(tail(tail(tail((expr))))), cf, env, counter);
    fprintf(cf, "))");
    return counter;
  }
  if (is_macro(expr, "c-apply")) {
    fprintf(cf, "%s(", head(tail(expr)).value.sval);
    NseVal args = tail(tail(expr));
    while (args.type == TYPE_CONS) {
      counter = write_expr(args.value.cval->h, cf, env, counter);
      args = args.value.cval->t;
      if (args.type == TYPE_CONS) {
        fprintf(cf, ", ");
      }
    }
    fprintf(cf, ")");
    return counter;
  }
  if (is_macro(expr, "c-var")) {
    fprintf(cf, "%s", head(tail(expr)).value.sval);
    return counter;
  }
  if (is_macro(expr, "if")) {
    fprintf(cf, "(is_true(");
    counter = write_expr(head(tail(expr)), cf, env, counter);
    fprintf(cf, ") ? ");
    counter = write_expr(head(tail(tail(expr))), cf, env, counter);
    fprintf(cf, " : ");
    counter = write_expr(head(tail(tail(tail(expr)))), cf, env, counter);
    fprintf(cf, ")");
    return counter;
  }
  if (is_macro(expr, "quote")) {
    write_quote(tail(expr), cf);
    return counter;
  }
  if (is_macro(expr, "list")) {
      NseVal args = expr.value.cval->t;
      size_t argc = 0;
      while (args.type == TYPE_CONS) {
        fprintf(cf, "Cons(");
        counter = write_expr(args.value.cval->h, cf, env, counter);
        fprintf(cf, ", ");
        argc++;
        args = args.value.cval->t;
      }
      fprintf(cf, "Nil");
      for ( ; argc > 0; argc--) {
        fprintf(cf, ")");
      }
    return counter;
  }
  switch (expr.type) {
    case TYPE_UNDEFINED:
    case TYPE_NIL:
      // todo: error
      return counter;
    case TYPE_CONS:
      fprintf(cf, "nse_apply(");
      counter = write_expr(expr.value.cval->h, cf, env, counter);
      fprintf(cf, ", ");
      NseVal args = expr.value.cval->t;
      size_t argc = 0;
      while (args.type == TYPE_CONS) {
        fprintf(cf, "Cons(");
        counter = write_expr(args.value.cval->h, cf, env, counter);
        fprintf(cf, ", ");
        argc++;
        args = args.value.cval->t;
      }
      fprintf(cf, "Nil");
      for ( ; argc > 0; argc--) {
        fprintf(cf, ")");
      }
      fprintf(cf, ")");
      return counter;
    case TYPE_INT:
      fprintf(cf, "Int(%d)", expr.value.ival);
      return counter;
    case TYPE_SYMBOL:
      fprintf(cf, "__var_");
      write_symbol(cf, expr.value.sval);
      return counter;
    case TYPE_QUOTE:
      write_quote(expr.value.qval->quoted, cf);
      return counter;
    case TYPE_ARRAY:
    case TYPE_FUNC:
    default:
      return counter;
  }
}

int write_temp_expr(NseVal expr, FILE *cf, Env *env, int counter) {
  if (is_macro(expr, "'anon")) {
    int i = head(tail(expr)).value.ival;
    fprintf(cf, "  NseVal __temp_%d = Closure(__anon_%d, ", ++counter, i);
    int size = 0;
    if (env == NULL) {
      fprintf(cf, "NULL, 0);\n");
    } else {
      fprintf(cf, "(NseVal[]){");
      for (Env *var = env; var != NULL; var = var->next, size++) {
        if (size > 0) {
          fprintf(cf, ", ");
        }
        fprintf(cf, "__var_");
        write_symbol(cf, var->name);
      }
      fprintf(cf, "}, %d);\n", size);
    }
  } else if (is_macro(expr, "c-int")) {
    // TODO: error
  } else if (is_macro(expr, "c-op")) {
    // TODO: error
  } else if (is_macro(expr, "c-apply")) {
    fprintf(cf, "  NseVal __temp_%d = %s(", ++counter, head(tail(expr)).value.sval);
    NseVal args = tail(tail(expr));
    while (args.type == TYPE_CONS) {
      counter = write_expr(args.value.cval->h, cf, env, counter);
      args = args.value.cval->t;
      if (args.type == TYPE_CONS) {
        fprintf(cf, ", ");
      }
    }
    fprintf(cf, ");\n");
  } else if (is_macro(expr, "c-var")) {
    fprintf(cf, "  NseVal __temp_%d = ", ++counter);
    fprintf(cf, "%s", head(tail(expr)).value.sval);
    fprintf(cf, ";\n");
  } else if (is_macro(expr, "if")) {
    int a = write_temp_expr(head(tail(expr)), cf, env, counter);
    int b = a + 1;
    fprintf(cf, "  NseVal __temp_%d = Undefined;\n", b);
    fprintf(cf, "  if (is_true(__temp_%d)) {\n", a);
    int c = write_temp_expr(head(tail(tail((expr)))), cf, env, b);
    fprintf(cf, "  __temp_%d = __temp_%d;\n", b, c);
    fprintf(cf, "  } else {\n");
    int d = write_temp_expr(head(tail(tail(tail((expr))))), cf, env, c);
    fprintf(cf, "  __temp_%d = __temp_%d;\n", b, d);
    fprintf(cf, "  }\n");
    fprintf(cf, "  del_ref(__temp_%d);\n", a);
    counter = d + 1;
    fprintf(cf, "  NseVal __temp_%d = __temp_%d;\n", counter, b);
  } else if (is_macro(expr, "quote")) {
    fprintf(cf, "  NseVal __temp_%d = add_ref(", ++counter);
    write_quote(tail(expr), cf);
    fprintf(cf, ");\n");
  } else if (is_macro(expr, "list")) {
    NseVal args = tail(expr);
    size_t argc = list_length(args);
    int *argv = calloc(argc, sizeof(int));
    int i = 0;
    while (args.type == TYPE_CONS) {
      counter = write_temp_expr(args.value.cval->h, cf, env, counter);
      argv[i++] = counter;
      args = args.value.cval->t;
    }
    for (i = argc - 1; i >= 0; i--) {
      counter++;
      if (i == argc - 1) {
        fprintf(cf, "  NseVal __temp_%d = Cons(__temp_%d, Nil);\n", counter, argv[i]);
      } else {
        fprintf(cf, "  NseVal __temp_%d = Cons(__temp_%d, __temp_%d);\n", counter, argv[i], counter - 1);
        fprintf(cf, "  del_ref(__temp_%d);\n", counter - 1);
      }
      fprintf(cf, "  del_ref(__temp_%d);\n", argv[i]);
    }
    free(argv);
  } else {
    switch (expr.type) {
      case TYPE_UNDEFINED:
      case TYPE_NIL:
        // todo: error
        return counter;
      case TYPE_CONS: {
        int a = write_temp_expr(expr.value.cval->h, cf, env, counter);
        counter = a;
        NseVal args = tail(expr);
        size_t argc = list_length(args);
        int *argv = calloc(argc, sizeof(int));
        int i = 0;
        while (args.type == TYPE_CONS) {
          counter = write_temp_expr(args.value.cval->h, cf, env, counter);
          argv[i++] = counter;
          args = args.value.cval->t;
        }
        for (i = argc - 1; i >= 0; i--) {
          counter++;
          if (i == argc - 1) {
            fprintf(cf, "  NseVal __temp_%d = Cons(__temp_%d, Nil);\n", counter, argv[i]);
          } else {
            fprintf(cf, "  NseVal __temp_%d = Cons(__temp_%d, __temp_%d);\n", counter, argv[i], counter - 1);
            fprintf(cf, "  del_ref(__temp_%d);\n", counter - 1);
          }
          fprintf(cf, "  del_ref(__temp_%d);\n", argv[i]);
        }
        int b = counter;
        fprintf(cf, "  NseVal __temp_%d = nse_apply(__temp_%d, __temp_%d);\n", ++counter, a, b);
        free(argv);
        fprintf(cf, "  del_ref(__temp_%d);\n", a);
        fprintf(cf, "  del_ref(__temp_%d);\n", b);
        return counter;
      }
      case TYPE_INT:
        fprintf(cf, "  NseVal __temp_%d = Int(%d);\n", ++counter, expr.value.ival);
        return counter;
      case TYPE_SYMBOL:
        fprintf(cf, "  NseVal __temp_%d = add_ref(__var_", ++counter);
        write_symbol(cf, expr.value.sval);
        fprintf(cf, ");\n");
        return counter;
      case TYPE_QUOTE:
        fprintf(cf, "  NseVal __temp_%d = add_ref(", ++counter);
        write_quote(expr.value.qval->quoted, cf);
        fprintf(cf, ");\n");
        return counter;
      case TYPE_ARRAY:
      case TYPE_FUNC:
      default:
        return counter;
    }
  }
  return counter;
}

void write_params(NseVal params, FILE *cf) {
  while (params.type == TYPE_CONS) {
    fprintf(cf, "  NseVal __var_");
    write_symbol(cf, head(params).value.sval);
    fprintf(cf, " = head(args); args = tail(args);\n");
    params = tail(params);
  }
  if (params.type == TYPE_SYMBOL) {
    fprintf(cf, "  NseVal __var_");
    write_symbol(cf, params.value.sval);
    fprintf(cf, " = args;\n");
  }
}

void deref_params(NseVal params, FILE *cf) {
  while (params.type == TYPE_CONS) {
    fprintf(cf, "  del_ref(__var_");
    write_symbol(cf, head(params).value.sval);
    fprintf(cf, ");\n");
    params = tail(params);
  }
  if (params.type == TYPE_SYMBOL) {
    fprintf(cf, "  del_ref(__var_");
    write_symbol(cf, params.value.sval);
    fprintf(cf, ");\n");
  }
  fprintf(cf, "  del_ref(args);\n");
}

int write_lambda_defs(NseVal prim, FILE *cf, int counter, Env *env) {
  int i;
  if (is_macro(prim, "define")) {
    if (tail(tail(tail(prim))).type == TYPE_CONS) {
      Env *new_env = env_push_params(env, head(tail(tail(prim))));
      counter = write_lambda_defs(head(tail(tail(tail(prim)))), cf, counter, new_env);
      env_delete_until(new_env, env);
      return counter;
    }
  }
  if (is_macro(prim, "lambda")) {
    int this = counter;
    Env *new_env = env_push_params(env, head(tail(prim)));
    counter = write_lambda_defs(tail(tail(prim)), cf, ++counter, new_env);
    fprintf(cf, "static NseVal __anon_%d(NseVal args, NseVal env[]) {\n", this);
    i = 0;
    for (Env *var = env; var != NULL; var = var->next) {
      fprintf(cf, "  NseVal __var_");
      write_symbol(cf, var->name);
      fprintf(cf, " = env[%d];\n", i++);
    }
    write_params(head(tail(prim)), cf);
    int temp = write_temp_expr(head(tail(tail(prim))), cf, new_env, 0);
    fprintf(cf, "  NseVal __result = __temp_%d;\n", temp);
    fprintf(cf, "  return __result;\n}\n");
    prim.value.cval->h = Symbol("'anon");
    prim.value.cval->t = Cons(Int(this), Nil);
    env_delete_until(new_env, env);
    return counter;
  }
  switch (prim.type) {
    case TYPE_CONS:
      counter = write_lambda_defs(head(prim), cf, counter, env);
      return write_lambda_defs(tail(prim), cf, counter, env);
    case TYPE_ARRAY:
      for (i = 0; i < prim.value.aval->size; i++) {
        counter = write_lambda_defs(prim.value.aval->array[i], cf, counter, env);
      }
      return counter;
    default:
      return counter;
  }
}

void write_init(NseVal def, FILE *cf) {
  if (is_macro(def, "define")) {
    NseVal name = head(tail(def));
    fprintf(cf, "  __var_");
    write_symbol(cf, name.value.sval);
    fprintf(cf, " = ");
    if (tail(tail(tail(def))).type == TYPE_CONS) {
      fprintf(cf, "FUNC(");
      write_symbol(cf, name.value.sval);
      fprintf(cf, ")");
    } else {
      write_expr(head(tail(tail(def))), cf, NULL, 0);
    }
    fprintf(cf, ";\n");
  } else if (is_macro(def, "public")) {
    NseVal next = tail(def);
    while (next.type == TYPE_CONS) {
      write_init(head(next), cf);
      next = tail(next);
    }
  } else if (is_macro(def, "private")) {
    NseVal next = tail(def);
    while (next.type == TYPE_CONS) {
      write_init(head(next), cf);
      next = tail(next);
    }
  } else if (is_macro(def, "import")) {
    NseVal name = head(tail(def));
    fprintf(cf, "  init_");
    write_symbol(cf, name.value.sval);
    fprintf(cf, "();\n");
  } else {
    printf("warning: not a valid definition:\n");
    nse_print(def);
    printf("\n");
  }
}

void write_inits(NseVal module, FILE *cf) {
  if (!is_macro(module, "module")) {
    printf("warning: root-expression should be a module\n");
    return;
  }
  NseVal next = tail(module);
  while (next.type == TYPE_CONS) {
    write_init(head(next), cf);
    next = tail(next);
  }
}

void write_def(NseVal def, FILE *cf) {
  if (is_macro(def, "define")) {
    NseVal name = head(tail(def));
    if (tail(tail(tail(def))).type == TYPE_CONS) {
      fprintf(cf, "NseVal ");
      write_symbol(cf, name.value.sval);
      fprintf(cf, "(NseVal args) {\n");
      fprintf(cf, "  printf(\"enter ");
      write_symbol(cf, name.value.sval);
      fprintf(cf, "\\n\");\n");
      write_params(head(tail(tail(def))), cf);
      Env *env = env_push_params(NULL, head(tail(tail(def))));
      int temp = write_temp_expr(head(tail(tail(tail(def)))), cf, env, 0);
      fprintf(cf, "  NseVal __result = __temp_%d;\n", temp);
      env_delete_until(env, NULL);
      fprintf(cf, "  printf(\"exit ");
      write_symbol(cf, name.value.sval);
      fprintf(cf, "\\n\");\n");
      fprintf(cf, "  return __result;\n}\n");
    }
  } else if (is_macro(def, "public")) {
    NseVal next = tail(def);
    while (next.type == TYPE_CONS) {
      write_def(head(next), cf);
      next = tail(next);
    }
  } else if (is_macro(def, "private")) {
    NseVal next = tail(def);
    while (next.type == TYPE_CONS) {
      write_def(head(next), cf);
      next = tail(next);
    }
  } else if (is_macro(def, "import")) {
  } else {
    printf("warning: not a valid definition:\n");
    nse_print(def);
    printf("\n");
  }
}

void write_defs(NseVal module, FILE *cf) {
  if (!is_macro(module, "module")) {
    printf("warning: root-expression should be a module\n");
    return;
  }
  NseVal next = tail(module);
  while (next.type == TYPE_CONS) {
    write_def(head(next), cf);
    next = tail(next);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("usage: %s MODULE\n", argv[0]);
    return 1;
  }
  char *src_name = sappend(argv[1], ".lisp");
  char *c_name = sappend(argv[1], ".c");
  char *h_name = sappend(argv[1], ".h");
  FILE *src_file = fopen(src_name, "r");
  if (!src_file) {
    printf("could not open file: %s: %s\n", src_name, strerror(errno));
    return 2;
  }
  Stack *stack = open_stack(src_file);
  stack->file_name = src_name;
  NseVal source = parse_prim(stack);
  skip(stack);
  if (peek(stack) != EOF) {
    printf("error: %s:%zu:%zu: expected end of file\n", stack->file_name, stack->line, stack->column);
    return 3;
  }
  close_stack(stack);
  FILE *c_file = fopen(c_name, "w");
  if (!c_file) {
    printf("could not open file: %s: %s\n", c_name, strerror(errno));
    return 4;
  }
  FILE *h_file = fopen(h_name, "w");
  if (!h_file) {
    printf("could not open file: %s: %s\n", h_name, strerror(errno));
    fclose(c_file);
    return 5;
  }
  fprintf(c_file, "#include \"%s\"\n\n", h_name);
  fprintf(h_file, "#ifndef ");
  write_uppercase_symbol(h_file, argv[1]);
  fprintf(h_file, "_H\n#define ");
  write_uppercase_symbol(h_file, argv[1]);
  fprintf(h_file, "_H\n\n#include \"nsert.h\"\n\n");
  fprintf(h_file, "void init_");
  write_symbol(h_file, argv[1]);
  fprintf(h_file, "();\n");
  write_decls(source, c_file, h_file);
  fprintf(h_file, "\n#endif\n");
  fclose(h_file);
  write_lambda_decls(source, c_file, 0);
  write_lambda_defs(source, c_file, 0, NULL);
  fprintf(c_file, "\nvoid init_");
  write_symbol(c_file, argv[1]);
  fprintf(c_file, "() {\n");
  write_inits(source, c_file);
  fprintf(c_file, "}\n");
  write_defs(source, c_file);
  if (has_main) {
    fprintf(c_file, "\nint main(int argc, char *argv[]) {\n");
    // TODO: convert argv
    fprintf(c_file, "  init_");
    write_symbol(c_file, argv[1]);
    fprintf(c_file, "();\n");
    fprintf(c_file, "  NseVal status = __main(Nil);\n");
    fprintf(c_file, "  if (status.type == TYPE_INT) {;\n");
    fprintf(c_file, "    return status.value.ival;\n");
    fprintf(c_file, "  }\n");
    fprintf(c_file, "  del_ref(status);\n");
    fprintf(c_file, "  return 0;\n");
    fprintf(c_file, "}\n");
  }
  fclose(c_file);
  return 0;
}
