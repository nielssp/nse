#ifndef READ_H
#define READ_H

#include <stdio.h>

#include "nsert.h"

typedef struct stack Stack;

Stack *open_stack_file(FILE *file);
Stack *open_stack_string(const char *string);
void close_stack(Stack *s);

Syntax *parse_prim(Stack *input);

#endif
