test:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter -std=c99 test.c cps-re.c -o bin/test

clean:
	rm -rf bin
