#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "nsert.h"

#define MAX_LOOKAHEAD 2

typedef struct stack {
  FILE *file;
  char *file_name;
  size_t la;
  char la_buffer[MAX_LOOKAHEAD];
  size_t line;
  size_t column;
} stack_t;

typedef struct env env_t;
struct env {
  char *name;
  env_t *next;
};
env_t *env_push(env_t *env, char *name) {
  env_t *new = malloc(sizeof(env_t));
  new->next = env;
  new->name = name;
  return new;
}
env_t *env_push_params(env_t *env, nse_val_t params) {
  while (params.type == TYPE_CONS) {
    env = env_push(env, head(params).value.sval);
    params = tail(params);
  }
  if (params.type == TYPE_SYMBOL) {
    env = env_push(env, params.value.sval);
  }
  return env;
}
void env_delete_until(env_t *start, env_t *end) {
  while (start != end) {
    env_t *next = start->next;
    free(start);
    start = next;
  }
}

int has_main = 0;

stack_t *open_stack(FILE *file) {
  stack_t *s = malloc(sizeof(stack_t));
  s->file = file;
  s->la = 0;
  s->line = 1;
  s->column = 1;
  s->file_name = NULL;
  return s;
}

void close_stack(stack_t *s) {
  fclose(s->file);
  free(s);
}

int pop(stack_t *s) {
  int c;
  if (s->la > 0) {
    c = s->la_buffer[0];
    s->la--;
    for (int i =0; i < s->la; i++) {
      s->la_buffer[i] = s->la_buffer[i + 1];
    }
  } else {
    c = fgetc(s->file);
  }
  if (c == '\n') {
    s->line++;
    s->column = 1;
  } else {
    s->column++;
  }
  return c;
}

int peekn(size_t n, stack_t *s) {
  while (s->la < n) {
    int c = fgetc(s->file);
    if (c == EOF) {
      return EOF;
    }
    s->la_buffer[s->la] = (char)c;
    s->la++;
  }
  return s->la_buffer[n - 1];
}

char peek(stack_t *s) {
  return peekn(1, s);
}

int iswhite(int c) {
  return c == '\n' || c == '\r' || c == '\t' || c == ' ';
}

void skip(stack_t *input) {
  while (iswhite(peek(input))) {
    pop(input);
  }
}

nse_val_t parse_int(stack_t *input) {
  int value = 0;
  int sign = 1;
  if (peek(input) == '-') {
    sign = -1;
    pop(input);
  }
  while (isdigit(peek(input))) {
    value = value * 10 + pop(input) - '0';
  }
  return Int(sign * value);
}

nse_val_t parse_symbol(stack_t *input) {
  nse_val_t prim;
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

nse_val_t parse_list(stack_t *input);

nse_val_t parse_prim(stack_t *input) {
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
    nse_val_t list = parse_list(input);
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

nse_val_t parse_list(stack_t *input) {
  char c;
  skip(input);
  c = peek(input);
  if (c == EOF || c == ')') {
    return Nil;
  }
  nse_val_t head = parse_prim(input);
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

int is_list(nse_val_t p) {
  return p.type == TYPE_CONS || p.type == TYPE_NIL;
}

int is_macro(nse_val_t p, const char *name) {
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

void write_decl(nse_val_t def, FILE *cf, FILE *hf, int public) {
  if (is_macro(def, "define")) {
    FILE *f = public ? hf : cf;
    nse_val_t name = head(tail(def));
    fprintf(f, "/* define %s */\n", name.value.sval);
    if (tail(tail(tail(def))).type == TYPE_CONS) {
      fprintf(f, "nse_val_t ");
      write_symbol(f, name.value.sval);
      fprintf(f, "(nse_val_t);\n");
    }
    fprintf(cf, "nse_val_t __var_");
    write_symbol(cf, name.value.sval);
    fprintf(cf, ";\n");
    if (public) {
      fprintf(hf, "extern nse_val_t __var_");
      write_symbol(hf, name.value.sval);
      fprintf(hf, ";\n");
    }
  } else if (is_macro(def, "public")) {
    nse_val_t next = tail(def);
    while (next.type == TYPE_CONS) {
      write_decl(head(next), cf, hf, 1);
      next = tail(next);
    }
  } else if (is_macro(def, "private")) {
    nse_val_t next = tail(def);
    while (next.type == TYPE_CONS) {
      write_decl(head(next), cf, hf, 0);
      next = tail(next);
    }
  } else if (is_macro(def, "import")) {
    nse_val_t name = head(tail(def));
    fprintf(cf, "#include \"%s.h\"\n", name.value.sval);
  } else {
    printf("warning: not a valid definition:\n");
    nse_print(def);
    printf("\n");
  }
}

void write_decls(nse_val_t module, FILE *cf, FILE *hf) {
  if (!is_macro(module, "module")) {
    printf("warning: root-expression should be a module\n");
    return;
  }
  nse_val_t next = tail(module);
  while (next.type == TYPE_CONS) {
    write_decl(head(next), cf, hf, 1);
    next = tail(next);
  }
}

int write_lambda_decls(nse_val_t prim, FILE *cf, int counter) {
  int i;
  if (is_macro(prim, "lambda")) {
    fprintf(cf, "static nse_val_t __anon_%d(nse_val_t, nse_val_t env[]);\n", counter);
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

void write_quote(nse_val_t prim, FILE *cf) {
  switch (prim.type) {
    case TYPE_NIL:
      fprintf(cf, "Nil");
      break;
    case TYPE_CONS:
      fprintf(cf, "Cons(");
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
      fprintf(cf, "Quote(");
      write_quote(prim.value.qval->quoted, cf);
      fprintf(cf, ")");
      break;
  }
}

int write_expr(nse_val_t expr, FILE *cf, env_t *env, int counter) {
  if (is_macro(expr, "'anon")) {
    int i = head(tail(expr)).value.ival;
    fprintf(cf, "Closure(__anon_%d, ", i);
    int size = 0;
    if (env == NULL) {
      fprintf(cf, "NULL, 0)");
    } else {
      fprintf(cf, "(nse_val_t[]){");
      for (env_t *var = env; var != NULL; var = var->next, size++) {
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
    nse_val_t args = tail(tail(expr));
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
      nse_val_t args = expr.value.cval->t;
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
      nse_val_t args = expr.value.cval->t;
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

int write_temp_expr(nse_val_t expr, FILE *cf, env_t *env, int counter) {
  if (is_macro(expr, "'anon")) {
    int i = head(tail(expr)).value.ival;
    fprintf(cf, "  nse_val_t __temp_%d = Closure(__anon_%d, ", ++counter, i);
    int size = 0;
    if (env == NULL) {
      fprintf(cf, "NULL, 0)");
    } else {
      fprintf(cf, "(nse_val_t[]){");
      for (env_t *var = env; var != NULL; var = var->next, size++) {
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
    fprintf(cf, "  nse_val_t __temp_%d = %s(", ++counter, head(tail(expr)).value.sval);
    nse_val_t args = tail(tail(expr));
    while (args.type == TYPE_CONS) {
      counter = write_expr(args.value.cval->h, cf, env, counter);
      args = args.value.cval->t;
      if (args.type == TYPE_CONS) {
        fprintf(cf, ", ");
      }
    }
    fprintf(cf, ");\n");
  } else if (is_macro(expr, "c-var")) {
    fprintf(cf, "  nse_val_t __temp_%d = ", ++counter);
    fprintf(cf, "%s", head(tail(expr)).value.sval);
    fprintf(cf, ";\n");
  } else if (is_macro(expr, "if")) {
    int a = write_temp_expr(head(tail(expr)), cf, env, counter);
    int b = write_temp_expr(head(tail(tail((expr)))), cf, env, a);
    int c = write_temp_expr(head(tail(tail(tail((expr))))), cf, env, b);
    counter = c + 1;
    fprintf(cf, "  nse_val_t __temp_%d = is_true(", counter);
    fprintf(cf, "__temp_%d) ? __temp_%d : __temp_%d;\n", a, b, c);
    fprintf(cf, "  del_ref(__temp_%d);\n", a);
    fprintf(cf, "  del_ref(__temp_%d);\n", b);
    fprintf(cf, "  del_ref(__temp_%d);\n", c);
  } else if (is_macro(expr, "quote")) {
    fprintf(cf, "  nse_val_t __temp_%d = ", ++counter);
    write_quote(tail(expr), cf);
    fprintf(cf, ";\n");
  } else if (is_macro(expr, "list")) {
    nse_val_t args = tail(expr);
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
        fprintf(cf, "  nse_val_t __temp_%d = Cons(__temp_%d, Nil);\n", counter, argv[i]);
      } else {
        fprintf(cf, "  nse_val_t __temp_%d = Cons(__temp_%d, __temp_%d);\n", counter, argv[i], counter - 1);
      }
    }
    for (i = 0; i < argc; i++) {
      if (i > 1) {
        fprintf(cf, "  del_ref(__temp_%d);\n", counter - i);
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
        nse_val_t args = tail(expr);
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
            fprintf(cf, "  nse_val_t __temp_%d = Cons(__temp_%d, Nil);\n", counter, argv[i]);
          } else {
            fprintf(cf, "  nse_val_t __temp_%d = Cons(__temp_%d, __temp_%d);\n", counter, argv[i], counter - 1);
            fprintf(cf, "  del_ref(__temp_%d);\n", counter - 1);
          }
          fprintf(cf, "  del_ref(__temp_%d);\n", argv[i]);
        }
        int b = counter;
        fprintf(cf, "  nse_val_t __temp_%d = nse_apply(__temp_%d, __temp_%d);\n", ++counter, a, b);
        free(argv);
        fprintf(cf, "  del_ref(__temp_%d);\n", a);
        fprintf(cf, "  del_ref(__temp_%d);\n", b);
        return counter;
      }
      case TYPE_INT:
        fprintf(cf, "  nse_val_t __temp_%d = Int(%d);\n", ++counter, expr.value.ival);
        return counter;
      case TYPE_SYMBOL:
        fprintf(cf, "  nse_val_t __temp_%d = add_ref(__var_", ++counter);
        write_symbol(cf, expr.value.sval);
        fprintf(cf, ");\n");
        return counter;
      case TYPE_QUOTE:
        fprintf(cf, "  nse_val_t __temp_%d = ", ++counter);
        write_quote(expr.value.qval->quoted, cf);
        fprintf(cf, ";\n");
        return counter;
      case TYPE_ARRAY:
      case TYPE_FUNC:
      default:
        return counter;
    }
  }
  return counter;
}

void write_params(nse_val_t params, FILE *cf) {
  while (params.type == TYPE_CONS) {
    fprintf(cf, "  nse_val_t __var_");
    write_symbol(cf, head(params).value.sval);
    fprintf(cf, " = head(args); args = tail(args);\n");
    params = tail(params);
  }
  if (params.type == TYPE_SYMBOL) {
    fprintf(cf, "  nse_val_t __var_");
    write_symbol(cf, params.value.sval);
    fprintf(cf, " = args;\n");
  }
}

void deref_params(nse_val_t params, FILE *cf) {
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

int write_lambda_defs(nse_val_t prim, FILE *cf, int counter, env_t *env) {
  int i;
  if (is_macro(prim, "define")) {
    if (tail(tail(tail(prim))).type == TYPE_CONS) {
      env_t *new_env = env_push_params(env, head(tail(tail(prim))));
      counter = write_lambda_defs(head(tail(tail(tail(prim)))), cf, counter, new_env);
      env_delete_until(new_env, env);
      return counter;
    }
  }
  if (is_macro(prim, "lambda")) {
    int this = counter;
    env_t *new_env = env_push_params(env, head(tail(prim)));
    counter = write_lambda_defs(tail(tail(prim)), cf, ++counter, new_env);
    fprintf(cf, "static nse_val_t __anon_%d(nse_val_t args, nse_val_t env[]) {\n", this);
    i = 0;
    for (env_t *var = env; var != NULL; var = var->next) {
      fprintf(cf, "  nse_val_t __var_");
      write_symbol(cf, var->name);
      fprintf(cf, " = env[%d];\n", i++);
    }
    write_params(head(tail(prim)), cf);
    int temp = write_temp_expr(head(tail(tail(prim))), cf, new_env, 0);
    fprintf(cf, "  nse_val_t __result = __temp_%d;\n", temp);
    fprintf(cf, "  add_ref(__result);\n");
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

void write_init(nse_val_t def, FILE *cf) {
  if (is_macro(def, "define")) {
    nse_val_t name = head(tail(def));
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
    nse_val_t next = tail(def);
    while (next.type == TYPE_CONS) {
      write_init(head(next), cf);
      next = tail(next);
    }
  } else if (is_macro(def, "private")) {
    nse_val_t next = tail(def);
    while (next.type == TYPE_CONS) {
      write_init(head(next), cf);
      next = tail(next);
    }
  } else if (is_macro(def, "import")) {
    nse_val_t name = head(tail(def));
    fprintf(cf, "  init_");
    write_symbol(cf, name.value.sval);
    fprintf(cf, "();\n");
  } else {
    printf("warning: not a valid definition:\n");
    nse_print(def);
    printf("\n");
  }
}

void write_inits(nse_val_t module, FILE *cf) {
  if (!is_macro(module, "module")) {
    printf("warning: root-expression should be a module\n");
    return;
  }
  nse_val_t next = tail(module);
  while (next.type == TYPE_CONS) {
    write_init(head(next), cf);
    next = tail(next);
  }
}

void write_def(nse_val_t def, FILE *cf) {
  if (is_macro(def, "define")) {
    nse_val_t name = head(tail(def));
    if (tail(tail(tail(def))).type == TYPE_CONS) {
      fprintf(cf, "nse_val_t ");
      write_symbol(cf, name.value.sval);
      fprintf(cf, "(nse_val_t args) {\n");
      write_params(head(tail(tail(def))), cf);
      env_t *env = env_push_params(NULL, head(tail(tail(def))));
      int temp = write_temp_expr(head(tail(tail(tail(def)))), cf, env, 0);
      fprintf(cf, "  nse_val_t __result = __temp_%d;\n", temp);
      env_delete_until(env, NULL);
     // fprintf(cf, "  add_ref(__result);\n");
      fprintf(cf, "  return __result;\n}\n");
    }
  } else if (is_macro(def, "public")) {
    nse_val_t next = tail(def);
    while (next.type == TYPE_CONS) {
      write_def(head(next), cf);
      next = tail(next);
    }
  } else if (is_macro(def, "private")) {
    nse_val_t next = tail(def);
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

void write_defs(nse_val_t module, FILE *cf) {
  if (!is_macro(module, "module")) {
    printf("warning: root-expression should be a module\n");
    return;
  }
  nse_val_t next = tail(module);
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
  stack_t *stack = open_stack(src_file);
  stack->file_name = src_name;
  nse_val_t source = parse_prim(stack);
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
    fprintf(c_file, "  nse_val_t status = __main(Nil);\n");
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
