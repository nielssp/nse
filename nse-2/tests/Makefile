GCCARGS = -Wall -pedantic -std=c11 -g -I../
CC = clang $(GCCARGS)

test: read-test hash_map-test type-test
	./read-test
	./hash_map-test
	./type-test

read-test: read-test.c ../libnsert.a
	$(CC) -o $@ $^

hash_map-test: hash_map-test.c
	$(CC) -o $@ $^

type-test: type-test.c
	$(CC) -o $@ $^

clean:
	rm -f *.o *.a read-test
