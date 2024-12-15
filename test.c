#include "cps-re.h"
#include <stdio.h>
#include <string.h>

void test(char *regex, char *input, char *exp) {
  char *act = cpsre_matches(regex, input);

  if ((act == CPSRE_SYNTAX) != (exp == CPSRE_SYNTAX))
    printf("test failed: /%s/ parse\n", regex);
  if (act == CPSRE_SYNTAX || exp == CPSRE_SYNTAX)
    return;

  if ((act == NULL) != (exp == NULL))
    goto test_failed;
  if (act == NULL || exp == NULL)
    return;

  if (strncmp(input, exp, act - input) != 0 || exp[act - input] != '\0')
  test_failed:
    printf("test failed: /%s/ against '%s'\n", regex, input);
}

int main(void) {
  test("a*b+bc", "abbbbc", "abbbbc");
  test("zzz|b+c", "abbbbc", NULL);
  test("zzz|ab+c", "abbbbc", "abbbbc");
  test("a+b|cd", "abbbbc", "ab");
  test("ab+|cd", "abbbbc", "abbbb");
  test("\n", "\n", "\n");
  test("a-z+", "abbbbc", "abbbbc");
  test("a-z+", "abBBbc", "ab");
  test("\\.-4*", "./01234X", "./01234");
  test("5-\\?*", "56789:;<=>?X", "56789:;<=>?");
  test("\\(-\\+*", "()*+X", "()*+");
  test("\t-\r*", "\t\n\v\f\rX", "\t\n\v\f\r");
  test(".", "abc", "a");

  // potential edge cases (mostly from LTRE)
  test("abba", "abba", "abba");
  test("(a|b)+", "abba", "abba");
  test("(a|b)+", "abc", "ab");
  test(".*", "abba", "abba");
  test("\x61\\+", "a+", "a+");
  test("", "", "");
  test("()", "", "");
  test("()*", "", "");
  test("()+", "", "");
  test("()?", "", "");
  test(" ", " ", " ");
  test("", "\n", "");
  test("\n", "", NULL);
  test(".", "\n", "\n");
  test("\\\\n", "\n", NULL);
  test("(|n)(\n)", "\n", "\n");
  test("\r?\n", "\n", "\n");
  test("\r?\n", "\r\n", "\r\n");
  test("(a*)*", "a", "a");
  test("(a+)+", "aa", "aa");
  test("(a?)?", "", "");
  test("a+", "aa", "aa");
  test("a?", "aa", "a");
  test("(a+)?", "aa", "aa");
  test("(ba+)?", "baa", "baa");
  test("(ab+)?", "b", "");
  test("(a+b)?", "a", "");
  test("(a+a+)+", "a", NULL);
  test("a+", "", NULL);
  test("(a+|)+", "aa", "aa");
  test("(a+|)+", "", "");
  test("(a|b)?", "", "");
  test("(a|b)?", "a", "a");
  test("(a|b)?", "b", "b");

  // parse errors (mostly from LTRE)
  test("abc)", "", CPSRE_SYNTAX);
  test("(abc", "", CPSRE_SYNTAX);
  test("+a", "", CPSRE_SYNTAX);
  test("a|*", "", CPSRE_SYNTAX);
  test("\\x0", "", CPSRE_SYNTAX);
  test("\\zzz", "", CPSRE_SYNTAX);
  test("\\b", "", CPSRE_SYNTAX);
  test("\\t", "", CPSRE_SYNTAX);
  test("a**", "", CPSRE_SYNTAX);
  test("a+*", "", CPSRE_SYNTAX);
  test("a?*", "", CPSRE_SYNTAX);

  // greedy, lazy, possessive
  test("a*", "aa", "aa");
  test("a+", "aa", "aa");
  test("a?", "a", "a");
  test("a*a", "aa", "aa");
  test("a+a", "aa", "aa");
  test("a?a", "a", "a");
  test("a*?", "aa", "");
  test("a+?", "aa", "a");
  test("a??", "a", "");
  test("a*?a", "aa", "a");
  test("a+?a", "aa", "aa");
  test("a??a", "aa", "a");
  test("a*+", "aa", "aa");
  test("a++", "aa", "aa");
  test("a?+", "a", "a");
  test("a*+a", "aa", NULL);
  test("a++a", "aa", NULL);
  test("a?+a", "a", NULL);
  test("(a?+)++a", "aaa", NULL);
  test("(a++)?+a", "aaa", NULL);
}
