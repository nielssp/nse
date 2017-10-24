#include <stdlib.h>
#include <stdio.h>

void boot(char *stack, size_t offset) {
  char *prog_p = stack;
  char *stack_p = stack + offset;
  while (1) {
    switch (*prog_p) {
      case 'p':
        *((int *)stack_p) = *((int *)(prog_p + 1));
        stack_p += 4;
        prog_p += 5;
        break;
      case '+':
        *((int *)(stack_p - 8)) += *((int *)(stack_p - 4));
        stack_p -= 4;
        prog_p += 1;
        break;
      case '-':
        *((int *)(stack_p - 8)) -= *((int *)(stack_p - 4));
        stack_p -= 4;
        prog_p += 1;
        break;
      case '*':
        *((int *)(stack_p - 8)) *= *((int *)(stack_p - 4));
        stack_p -= 4;
        prog_p += 1;
        break;
      case '/':
        *((int *)(stack_p - 8)) *= *((int *)(stack_p - 4));
        stack_p -= 4;
        prog_p += 1;
        break;
      case 'd':
        *((int *)stack_p) = *((int *)(stack_p - 4));
        stack_p += 4;
        prog_p += 1;
        break;
      case 'j':
        prog_p = stack + *((int *)(prog_p + 1));
        break;
      case 'c':
        if (*((int *)(stack_p - 4))) {
          prog_p = stack + *((int *)(prog_p + 1));
        } else {
          prog_p += 5;
        }
        stack_p -= 4;
        break;
      case 'q':
        printf("Terminated. Stack contents:\n");
        char *start = stack + offset;
        while (start < stack_p) {
          printf("%p: %08x\n", start, *((int *)start));
          start += 4;
        }
        return;
    }
  }
}

int main(int argc, char *argv[]) {
  FILE *f = fopen(argv[1], "r");
  printf("Initializing vm...\n");
  char *stack = malloc(1024 * 1024 * 1024);
  size_t stackp = 0;
  size_t read = 0;
  do {
    read = fread(stack + stackp, 1, 8192, f);
    stackp += read;
  } while (read);
  fclose(f);
  printf("Stack initialized. Current stack pointer: %zd\n", stackp);
  boot(stack, stackp);
  return 0;
}
