GCCARGS = -Wall -pedantic -std=c11 -g
CC = clang $(GCCARGS)
MAIN_LIBS = -lreadline -ldl

src = $(wildcard *.c) util/stream.c
obj = $(src:.c=.o)

runtime_src = $(wildcard runtime/*.c)
runtime_obj = $(runtime_src:.c=.o)

modc: $(obj) libnsert.a
	$(CC) -o $@ $^ $(LDFLAGS)

libnsert.a: $(runtime_obj)
	ar rcs $@ $^

.PHONY: clean
clean:
	rm -f $(obj) modc
