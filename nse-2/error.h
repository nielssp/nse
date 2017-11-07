#ifndef ERROR_H
#define ERROR_H

typedef struct symbol Symbol;

extern Symbol *out_of_memory_error;
extern Symbol *domain_error;
extern Symbol *name_error;
extern Symbol *io_error;
extern Symbol *syntax_error;

void init_error_module();
void raise_error(Symbol *error_type, const char *format, ...);
const char *current_error();
Symbol *current_error_type();
void clear_error();
void *allocate(size_t bytes);

#endif
