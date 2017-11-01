#ifndef ERROR_H
#define ERROR_H

void raise_error(const char *format, ...);
const char *current_error();
void clear_error();
void *allocate(size_t bytes);

#endif
