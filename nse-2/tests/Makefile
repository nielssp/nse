GCCARGS = -Wall -pedantic -std=c11 -g -I../
CC = clang $(GCCARGS)

test: read-test
	./read-test

read-test: read-test.c ../libnsert.a
	$(CC) -o $@ $^

clean:
	rm -f *.o *.a read-test
