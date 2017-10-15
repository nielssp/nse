GCCARGS = -Wall -pedantic -std=c11 -g
CC = clang $(GCCARGS)
MAIN_LIBS = -lreadline

all: nse

test: all
	make -C ./tests test

run: testprog
	-./testprog ||:

testprog: testprog.c std.o libnsert.a
	$(CC) -o $@ $^

testprog.c: nsec testprog.lisp
	./nsec testprog

std.o: std.c
	$(CC) -c -o $@ $^

std.c: nsec std.lisp
	./nsec std

nsec: nsec.o libnsert.a
	$(CC) -o $@ $^

nsec.o: nsec.c
	$(CC) -c -o $@ $<

nse: main.o read.o eval.o libnsert.a
	$(CC) $(MAIN_LIBS) -o $@ $^

main.o: main.c
	$(CC) -c -o $@ $<

eval.o: eval.c
	$(CC) -c -o $@ $<

read.o: read.c
	$(CC) -c -o $@ $<

libnsert.a: nsert.o hash_map.o
	ar rcs $@ $^

nsert.o: nsert.c nsert.h
	$(CC) -c -o $@ $<

hash_map.o: util/hash_map.c util/hash_map.h
	$(CC) -c -o $@ $<

clean:
	rm -f *.o *.a nsec testprog nse
