TARGET = nse
GCCARGS = -Wall -pedantic -std=c11 -g
CC = clang $(GCCARGS)
LDFLAGS = -lreadline

src = $(wildcard src/*.c) src/util/stream.c
obj = $(src:.c=.o)

runtime_src = $(wildcard src/runtime/*.c)
runtime_obj = $(runtime_src:.c=.o)

test_src = $(wildcard tests/*.c)
test_obj = $(test_src:.c=)

$(TARGET): $(obj) libnsert.a
	$(CC) -o $@ $^ $(LDFLAGS)

libnsert.a: $(runtime_obj)
	ar rcs $@ $^

.PHONY: clean
clean:
	rm -f $(obj) $(runtime_obj) $(test_obj) $(TARGET)
