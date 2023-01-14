build/test: build build/test.o build/unicode.o
	cc -o build/test build/*.o

build/test.o: test.c
	cc -o build/test.o -c test.c

build/unicode.o: unicode.c
	cc --std=c2x -o build/unicode.o -c unicode.c

build:
	mkdir build
