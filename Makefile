GCCARGS = -Wall -pedantic -std=c11 -g
CC = clang $(GCCARGS)
LDFLAGS = -lreadline

src = $(wildcard src/*.c) src/util/stream.c
obj = $(src:.c=.o)

runtime_src = $(wildcard src/runtime/*.c)
runtime_obj = $(runtime_src:.c=.o)

nse: $(obj) libnsert.a
	$(CC) -o $@ $^ $(LDFLAGS)

libnsert.a: $(runtime_obj)
	ar rcs $@ $^

.PHONY: clean
clean:
	rm -f $(obj) nse
