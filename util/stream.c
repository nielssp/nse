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
  STREAM_TYPE_BUFFER,
  STREAM_TYPE_STRING
} StreamType;

struct stream {
  StreamType type;
  size_t length;
  size_t pos;
  union {
    FILE *file;
    char *buffer;
    const char *string;
  };
};

Stream *stream_stdin() {
  static Stream *s = NULL;
  if (!s) {
    s = malloc(sizeof(Stream));
    s->type = STREAM_TYPE_FILE_NOCLOSE;
    s->file = stdin;
  }
  return s;
}

Stream *stream_stdout() {
  static Stream *s = NULL;
  if (!s) {
    s = malloc(sizeof(Stream));
    s->type = STREAM_TYPE_FILE_NOCLOSE;
    s->file = stdout;
  }
  return s;
}

Stream *stream_stderr() {
  static Stream *s = NULL;
  if (!s) {
    s = malloc(sizeof(Stream));
    s->type = STREAM_TYPE_FILE_NOCLOSE;
    s->file = stderr;
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
  stream->file = file;
  return stream;
}

Stream *stream_buffer(char *buffer, size_t length) {
  Stream *stream = (Stream *)malloc(sizeof(Stream));
  stream->type = STREAM_TYPE_BUFFER;
  stream->buffer = buffer;
  stream->length = length;
  stream->pos = 0;
  return stream;
}

Stream *stream_string(const char *string) {
  Stream *stream = (Stream *)malloc(sizeof(Stream));
  stream->type = STREAM_TYPE_STRING;
  stream->string = string;
  stream->pos = 0;
  return stream;
}

char *stream_get_content(Stream *stream) {
  switch (stream->type) {
    case STREAM_TYPE_FILE:
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_STRING:
      return NULL;
    case STREAM_TYPE_BUFFER:
      return stream->buffer;
  }
}

size_t stream_get_size(Stream *stream) {
  switch (stream->type) {
    case STREAM_TYPE_FILE:
    case STREAM_TYPE_FILE_NOCLOSE:
      return 0;
    case STREAM_TYPE_BUFFER:
      return stream->length;
    case STREAM_TYPE_STRING:
      return strlen(stream->string);
  }
}

void stream_close(Stream *stream) {
  switch (stream->type) {
    case STREAM_TYPE_FILE:
      fclose(stream->file);
      break;
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_BUFFER:
    case STREAM_TYPE_STRING:
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
      return fread(ptr, size, nmemb, input->file);
    case STREAM_TYPE_BUFFER:
      bytes = size * nmemb;
      remaining = input->length - input->pos;
      if (remaining <= 0) {
        return 0;
      }
      else if (remaining < bytes) {
        char *src = input->buffer + input->pos;
        memcpy(ptr, src, remaining);
        input->pos += remaining;
        return remaining;
      }
      else {
        char *src = input->buffer + input->pos;
        memcpy(ptr, src, bytes);
        input->pos += bytes;
        return bytes;
      }
    case STREAM_TYPE_STRING:
      bytes = size * nmemb;
      char *dest = (char *)ptr;
      size_t i = 0;
      while (i < bytes) {
        if (input->string[input->pos] == 0) {
          break;
        }
        dest[i++] = input->string[input->pos++];
      }
      return i;
  }
}

int stream_getc(Stream *input) {
  switch (input->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      return fgetc(input->file);
    case STREAM_TYPE_BUFFER:
      if (input->pos >= input->length) {
        return EOF;
      }
      return input->buffer[input->pos++];
    case STREAM_TYPE_STRING:
      if (input->string[input->pos] == 0) {
        return EOF;
      }
      return input->string[input->pos++];
  }
}

void stream_ungetc(int c, Stream *input) {
  switch (input->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      ungetc(c, input->file);
      break;
    case STREAM_TYPE_BUFFER:
      if (input->pos > input->length) {
        input->pos = input->length;
      }
      input->buffer[--input->pos] = (char) c;
      break;
    case STREAM_TYPE_STRING:
      if (input->pos > 0) {
        input->pos--;
      }
      break;
  }
}

int stream_eof(Stream *input) {
  switch (input->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      return feof(input->file);
    case STREAM_TYPE_BUFFER:
      return input->pos >= input->length;
    case STREAM_TYPE_STRING:
      return input->string[input->pos] == 0;
  }
}

int stream_putc(int c, Stream *output) {
  unsigned char ch = (unsigned char)c;
  switch (output->type) {
    case STREAM_TYPE_FILE_NOCLOSE:
    case STREAM_TYPE_FILE:
      return fputc(c, output->file);
    case STREAM_TYPE_BUFFER:
      if (output->pos >= output->length) {
        output->buffer = resize_buffer(output->buffer, output->length, output->length + 100);
        output->length += 100;
      }
      output->buffer[output->pos++] = ch;
      return ch;
    case STREAM_TYPE_STRING:
      return EOF;
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
      status = vfprintf(output->file, format, va2);
      va_end(va2);
      break;
    case STREAM_TYPE_BUFFER:
      size = output->length - output->pos;
      if (size <= 0) {
        output->buffer = resize_buffer(output->buffer, output->length, output->length + 100);
        output->length += 100;
      }
      while (1) {
        char *dest = output->buffer + output->pos;
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
        output->buffer = resize_buffer(output->buffer, output->length, size + output->pos);
        output->length = size + output->pos;
      }
      break;
    case STREAM_TYPE_STRING:
      return EOF;
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

char *string_copy(const char *str) {
  size_t len = strlen(str);
  char *copy = malloc(len + 1);
  memcpy(copy, str, len + 1);
  return copy;
}
