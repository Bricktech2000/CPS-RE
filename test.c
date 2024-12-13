#include "cps-re.h"
#include <stdio.h>

int main(void) {
  char *res[] = {"no match", "syntax", "match"};
  puts(res[matches("a*b+bc", "abbbbc")]);
  puts(res[matches("a+*", "abbbbc")]);
  puts(res[matches("zzz|b+c", "abbbbc")]);
  puts(res[matches("zzz|ab+c", "abbbbc")]);
  puts(res[matches("a+b|cd", "abbbbc")]);
  puts(res[matches("\\n", "abbbbc")]);
}
