CC=gcc
CFLAGS=-std=c99 -Wall -g -D_DEFAULT_SOURCE
LDFLAGS=-g
LDLIBS=-lm

all: fish cmdline_test

fish: fish.o libcmdline.so
	$(CC) $(CFLAGS) -L. $< -o $@ -lcmdline

fish.o: fish.c
	$(CC) $(CFLAGS) -c -o $@ $^

cmdline.o: cmdline.c cmdline.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

libcmdline.so: cmdline.o
	$(CC) $(CFLAGS) -shared $^ -o $@

cmdline_test.o: cmdline_test.c
	$(CC) $(CFLAGS) -c -o $@ $^

cmdline_test: cmdline_test.o libcmdline.so
	$(CC) $(CFLAGS) -L. $< -o $@ -lcmdline

clean:
	rm -f *.o

mrproper: clean
	rm -f fish cmdline_test *.so
