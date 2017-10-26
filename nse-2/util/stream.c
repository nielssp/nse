/* ctodo
 * Copyright (c) 2016 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include <stdio.h>
#include <string.h>

#include "stream.h"

typedef enum {
  STREAM_TYPE_FILE,
  STREAM_TYPE_FILE_NOCLOSE,
  STREAM_TYPE_BUFFER
} StreamType;

struct stream {
  StreamType type;
  size_t length;
  size_t pos;
  void *obj;
};

Stream *stream_stdin() {
  static Stream *s = NULL;
  if (!s) {
    s = malloc(sizeof(Stream));
    s->type = STREAM_TYPE_FILE_NOCLOSE;
    s->obj = stdin;
  }
  return s;
}

Stream *stream_stdout() {
  static Stream *s = NULL;
  if (!s) {
    s = malloc(sizeof(Stream));
    s->type = STREAM_TYPE_FILE_NOCLOSE;
    s->obj = stdout;
  }
  return s;
}

Stream *stream_stderr() {
  static Stream *s = NULL;
  if (!s) {
    s = malloc(sizeof(Stream));
    s->type = STREAM_TYPE_FILE_NOCLOSE;
    s->obj = stderr;
  }
  return s;
}

Stream *stream_file(const char *filename, const char *mode) {
  FILE *file = fopen(filename, mode);
  Stream *stream = NULL;
  if (!file) {
    return NULL;
  }
  stream = (Stream *)malloc(sizeof(Stream));
  stream->type = STREAM_TYPE_FILE;
  stream->obj = file;
  return stream;
}

Stream *stream_buffer(char *buffer, size_t length) {
  Stream *stream = (Stream *)malloc(sizeof(Stream));
  stream->type = STREAM_TYPE_BUFFER;
  stream->obj = buffer;
  stream->length = length;
  stream->pos = 0;
  return stream;
}

char *stream_get_content(Stream *stream) {
  switch (stream->type) {
    case STREAM_TYPE_FILE:
    case STREAM_TYPE_FILE_NOCLOSE:
      return NULL;
    case STREAM_TYPE_BUFFER:
      return stream->obj;
  }
}

size_t stream_get_size(Stream *stream) {
  switch (stream->type) {
    case STREAM_TYPE_FILE:
    case STREAM_TYPE_FILE_NOCLOSE:
      return 0;
    case STREAM_TYPE_BUFFER:
      return stream->length;
  }
}

void stream_close(Stream *stream) {
  switch (stream->type) {
    case STREAM_TYPE_FILE:
      fclose(stream->obj);
      break;
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_BUFFER:
      break;
  }
  free(stream);
}

char *resize_buffer(char *buffer, size_t oldsize, size_t newsize) {
  char *new = NULL;
  if (newsize < oldsize) {
    return NULL;
  }
  new = (char *)malloc(newsize);
  if (!new) {
    return NULL;
  }
  memcpy(new, buffer, oldsize);
  free(buffer);
  return new;
}

size_t stream_read(void *ptr, size_t size, size_t nmemb, Stream *input) {
  size_t bytes, remaining;
  switch (input->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      return fread(ptr, size, nmemb, input->obj);
    case STREAM_TYPE_BUFFER:
      bytes = size * nmemb;
      remaining = input->length - input->pos;
      if (remaining <= 0) {
        return 0;
      }
      else if (remaining < bytes) {
        char *src = (char *)input->obj + input->pos;
        memcpy(ptr, src, remaining);
        input->pos += remaining;
        return remaining;
      }
      else {
        char *src = (char *)input->obj + input->pos;
        memcpy(ptr, src, bytes);
        input->pos += bytes;
        return bytes;
      }
  }
}

int stream_getc(Stream *input) {
  switch (input->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      return fgetc(input->obj);
    case STREAM_TYPE_BUFFER:
      if (input->pos >= input->length) {
        return EOF;
      }
      char *buffer = (char *)input->obj;
      return buffer[input->pos++];
  }
}

void stream_ungetc(int c, Stream *input) {
  switch (input->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      ungetc(c, input->obj);
      break;
    case STREAM_TYPE_BUFFER:
      if (input->pos > input->length) {
        input->pos = input->length;
      }
      char *buffer = (char *)input->obj;
      buffer[--input->pos] = (char) c;
      break;
  }
}

int stream_eof(Stream *input) {
  switch (input->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      return feof((FILE *)input->obj);
    case STREAM_TYPE_BUFFER:
      return input->pos >= input->length;
  }
}

int stream_putc(int c, Stream *output) {
  unsigned char ch = (unsigned char)c;
  switch (output->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      return fputc(c, output->obj);
    case STREAM_TYPE_BUFFER:
      if (output->pos >= output->length) {
        output->obj = resize_buffer(output->obj, output->length, output->length + 100);
        output->length += 100;
      }
      ((char *)output->obj)[output->pos++] = ch;
      return ch;
  }
}

int stream_vprintf(Stream *output, const char *format, va_list va) {
  int status = 0, n;
  va_list va2;
  size_t size;
  switch (output->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      va_copy(va2, va);
      status = vfprintf(output->obj, format, va2);
      va_end(va2);
      break;
    case STREAM_TYPE_BUFFER:
      size = output->length - output->pos;
      if (size <= 0) {
        output->obj = resize_buffer(output->obj, output->length, output->length + 100);
        output->length += 100;
      }
      while (1) {
        char *dest = (char *)output->obj + output->pos;
        va_copy(va2, va);
        n = vsnprintf(dest, size, format, va2);
        va_end(va2);
        if (n < 0) {
          return 0;
        }
        if (n < size) {
          output->pos += n;
          break;
        }
        size = n + 1;
        output->obj = resize_buffer(output->obj, output->length, size + output->pos);
        output->length = size + output->pos;
      }
      break;
  }
  return status;
}

int stream_printf(Stream *output, const char *format, ...) {
  va_list va;
  int status;
  va_start(va, format);
  status = stream_vprintf(output, format, va);
  va_end(va);
  return status;
}

char *string_vprintf(const char *format, va_list va) {
  size_t size = 32;
  char *buffer = (char *)malloc(size);
  va_list va2;
  Stream *stream = stream_buffer(buffer, size);
  va_copy(va2, va);
  stream_vprintf(stream, format, va2);
  va_end(va2);
  buffer = stream_get_content(stream);
  stream_close(stream);
  return buffer;
}

char *string_printf(const char *format, ...) {
  char *result = NULL;
  va_list va;
  va_start(va, format);
  result = string_vprintf(format, va);
  va_end(va);
  return result;
}
