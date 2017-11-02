#ifndef READ_H
#define READ_H

#include "nsert.h"
#include "util/stream.h"

typedef struct reader Reader;

Reader *open_reader(Stream *stream, const char *file_name);
void close_reader(Reader *reader);

Syntax *nse_read(Reader *reader);

#endif
