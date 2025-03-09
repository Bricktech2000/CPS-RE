CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: bin/test

bin/test: test.c bin/cps-re.o
	mkdir -p bin/
	$(CC) $(CFLAGS) $^ -o $@

bin/cps-re.o: cps-re.c cps-re.h
	mkdir -p bin/
	$(CC) $(CFLAGS) -Wno-unused-parameter -Wno-unused-value -Wno-clobbered -c $< -o $@

clean:
	rm -rf bin/
