.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: bin/test
bin/:; mkdir bin/
clean:; rm -rf bin/

bin/test:     bin/cps-re.o test.c;    $(CC) $(CFLAGS) -o $@ bin/cps-re.o test.c
bin/cps-re.o: bin/ cps-re.h cps-re.c; $(CC) $(CFLAGS) -o $@ -c cps-re.c -Wno-unused-parameter -Wno-unused-value -Wno-bool-operation -Wno-clobbered
