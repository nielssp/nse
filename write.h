#ifndef WRITE_H
#define WRITE_H

#include "runtime/value.h"
#include "util/stream.h"

NseVal nse_write(NseVal value, Stream *stream, Module *module);
char *nse_write_to_string(NseVal value, Module *module);

#endif
