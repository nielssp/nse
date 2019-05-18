TARGET = nse
GCCARGS = -Wall -pedantic -std=c11 -g
CC = clang $(GCCARGS)
LDFLAGS = -lreadline

src = $(wildcard *.c) ../src/util/stream.c
obj = $(src:.c=.o)

$(TARGET): $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(TARGET)
