#ifndef READ_H
#define READ_H

#include "runtime/value.h"
#include "module.h"
#include "util/stream.h"

typedef struct reader Reader;

Reader *open_reader(Stream *stream, const char *file_name, Module *module);
void set_reader_module(Reader *reader, Module *module);
void set_reader_position(Reader *reader, size_t line, size_t column);
void get_reader_position(Reader *reader, String **file_name, size_t *line, size_t *column);
void close_reader(Reader *reader);

Syntax *nse_read(Reader *reader);

int iswhite(int c);

NseVal execute_read(Reader *reader, NseVal read, int *skip);

#endif
