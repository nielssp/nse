/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef READ_H
#define READ_H

#include "value.h"
#include "../src/util/stream.h"

typedef struct Module Module;
typedef struct Reader Reader;

Reader *open_reader(Stream *stream, const char *file_name, Module *module);
void set_reader_module(Reader *reader, Module *module);
void set_reader_position(Reader *reader, size_t line, size_t column);
void get_reader_position(Reader *reader, String **file_name, size_t *line, size_t *column);
void close_reader(Reader *reader);

Syntax *nse_read(Reader *reader);

int iswhite(int c);

Value execute_read(Reader *reader, Value read, int *skip);

#endif
