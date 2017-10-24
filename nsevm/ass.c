#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void skip_ws(FILE *f) {
  int c = fgetc(f);
  while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
    c = fgetc(f);
  }
  ungetc(c, f);
}

char *read_token(FILE *f) {
  char *token = malloc(32);
  char l = 0;
  while (l < 31) {
    int c = fgetc(f);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == EOF || c == ':') {
      ungetc(c, f);
      break;
    }
    token[l++] = (char)c;
  }
  if (l == 0) {
    free(token);
    return NULL;
  }
  token[l] = 0;
  return token;
}

struct offset {
  size_t offset;
  struct offset *next;
};

struct label {
  char *name;
  char seen;
  size_t target;
  struct offset *pending;
  struct label *next;
};

int contains_label(struct label *stack, char *name, size_t target) {
  if (strcmp(name, stack->name) == 0) {
    if (stack->seen) {
      printf("multiple definitions of label: %s\n", name);
    }
    stack->target = target;
    stack->seen = 1;
    return 1;
  } else if (stack->next) {
    return contains_label(stack->next, name, target);
  }
  return 0;
}

struct label *push_label(struct label *stack, char *name, size_t target) {
  if (contains_label(stack, name, target)) {
    return stack;
  }
  struct label *new = malloc(sizeof(struct label));
  new->name = name;
  new->target = target;
  new->seen = 1;
  new->pending = NULL;
  new->next = stack;
  return new;
}

void push_pending(struct label *stack, size_t prog_p) {
  struct offset *off = malloc(sizeof(struct offset));
  off->offset = prog_p;
  off->next = stack->pending;
  stack->pending = off;
}

int get_label(struct label *stack, char *name, size_t prog_p, size_t *target) {
  if (strcmp(name, stack->name) == 0) {
    if (!stack->seen) {
      push_pending(stack, prog_p);
    }
    *target = stack->target;
    return 1;
  } else if (stack->next) {
    return get_label(stack->next, name, prog_p, target);
  }
  return 0;
}

size_t get_or_add_label(struct label **stack, char *name, size_t prog_p) {
  size_t target;
  if (*stack && get_label(*stack, name, prog_p, &target)) {
    return target;
  } else {
    struct label *new = malloc(sizeof(struct label));
    new->name = name;
    new->target = 0;
    new->seen = 0;
    new->pending = NULL;
    new->next = *stack;
    *stack = new;
    push_pending(*stack, prog_p);
    return 0;
  }
}

int main(int argc, char *argv[]) {
  FILE *in = fopen(argv[2], "r");
  FILE *out = fopen(argv[1], "w");
  struct label *labels = NULL;
  size_t prog_p = 0;
  skip_ws(in);
  while (1) {
    char *token = read_token(in);
    if (token == NULL) {
      break;
    }
    skip_ws(in);
    int c = fgetc(in);
    if (c == ':') {
      labels = push_label(labels, token, prog_p);
      skip_ws(in);
      continue;
    } else {
      ungetc(c, in);
    }
    if (strcmp(token, "jump") == 0 || strcmp(token, "cjump") == 0) {
      char *target = read_token(in);
      fputc(token[0], out);
      prog_p += 1;
      size_t addr = get_or_add_label(&labels, target, prog_p);
      fputc((addr >>  0) & 0xFF, out);
      fputc((addr >>  8) & 0xFF, out);
      fputc((addr >> 16) & 0xFF, out);
      fputc((addr >> 24) & 0xFF, out);
      prog_p += 4;
    } else if (strcmp(token, "add") == 0) {
      fputc('+', out);
      prog_p += 1;
    } else if (strcmp(token, "sub") == 0) {
      fputc('-', out);
      prog_p += 1;
    } else if (strcmp(token, "mult") == 0) {
      fputc('*', out);
      prog_p += 1;
    } else if (strcmp(token, "div") == 0) {
      fputc('/', out);
      prog_p += 1;
    } else if (strcmp(token, "dup") == 0) {
      fputc('d', out);
      prog_p += 1;
    } else if (strcmp(token, "push") == 0) {
      char *param = read_token(in);
      fputc('p', out);
      prog_p += 1;
      int i = atoi(param);
      fputc((i >>  0) & 0xFF, out);
      fputc((i >>  8) & 0xFF, out);
      fputc((i >> 16) & 0xFF, out);
      fputc((i >> 24) & 0xFF, out);
      prog_p += 4;
    } else if (strcmp(token, "quit") == 0) {
      fputc('q', out);
      prog_p += 1;
    }
    skip_ws(in);
  }
  while (labels) {
    struct label *l = labels;
    if (!l->seen) {
      printf("undefined label: %s\n", l->name);
    } else {
      while (l->pending) {
        struct offset *off = l->pending;
        printf("write pending label %s:%zd at %zd\n", l->name, l->target, off->offset);
        fseek(out, off->offset, SEEK_SET);
        fputc((l->target >>  0) & 0xFF, out);
        fputc((l->target >>  8) & 0xFF, out);
        fputc((l->target >> 16) & 0xFF, out);
        fputc((l->target >> 24) & 0xFF, out);
        l->pending = off->next;
        free(off);
      }
    }
    labels = l->next;
    free(l);
  }
  fclose(in);
  fclose(out);
}
