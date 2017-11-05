GCCARGS = -Wall -pedantic -std=c11 -g
CC = clang $(GCCARGS)
MAIN_LIBS = -lreadline -ldl

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

nse: main.o read.o write.o eval.o rtci.o system.o libnsert.a stream.o
	$(CC) $(MAIN_LIBS) -o $@ $^

main.o: main.c
	$(CC) -c -o $@ $<

rtci.o: rtci.c
	$(CC) -c -o $@ $<

eval.o: eval.c
	$(CC) -c -o $@ $<

write.o: write.c
	$(CC) -c -o $@ $<

read.o: read.c
	$(CC) -c -o $@ $<

system.o: system.c
	$(CC) -c -o $@ $<

libnsert.a: nsert.o hash_map.o type.o error.o module.o lang.o
	ar rcs $@ $^

lang.o: lang.c lang.h
	$(CC) -c -o $@ $<

module.o: module.c module.h
	$(CC) -c -o $@ $<

nsert.o: nsert.c nsert.h
	$(CC) -c -o $@ $<

type.o: type.c type.h
	$(CC) -c -o $@ $<

error.o: error.c error.h
	$(CC) -c -o $@ $<

stream.o: util/stream.c util/stream.h
	$(CC) -c -o $@ $<

hash_map.o: util/hash_map.c util/hash_map.h
	$(CC) -c -o $@ $<

clean:
	rm -f *.o *.a nsec testprog nse
