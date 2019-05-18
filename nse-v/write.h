/* SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Niels Sonnich Poulsen (http://nielssp.dk)
 */
#ifndef NSE_WRITE_H
#define NSE_WRITE_H

#include "value.h"
#include "../src/util/stream.h"

Value nse_write(Value value, Stream *stream, Module *module, int max_nesting);

char *nse_write_to_string(Value value, Module *module);

#endif
