GCCARGS = -Wall -pedantic -std=c99 -g
CC = clang $(GCCARGS)

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

libnsert.a: nsert.o hash_map.o
	ar rcs $@ $^

nsert.o: nsert.c nsert.h
	$(CC) -c -o $@ $<

hash_map.o: util/hash_map.c util/hash_map.h
	$(CC) -c -o $@ $<

clean:
	rm -f *.o *.a nsec testprog
