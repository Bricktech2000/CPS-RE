.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: test
clean:; rm -f test cps-re.o

test:     cps-re.o test.c;   $(CC) $(CFLAGS) -o $@ cps-re.o test.c
cps-re.o: cps-re.h cps-re.c; $(CC) $(CFLAGS) -o $@ -c cps-re.c -Wno-unused-parameter -Wno-unused-value -Wno-bool-operation -Wno-clobbered
