#include "cps-re.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dump(char *begin, char *end, char delim) {
  if (begin == CPSRE_SYNTAX)
    printf("syntax error");
  else if (begin == NULL)
    printf("no match");
  else {
    putchar(delim);
    for (; end == NULL ? *begin : begin < end; begin++)
      printf(isprint(*begin) && *begin != '\\' ? "%c" : "\\x%02hhx", *begin);
    putchar(delim);
  }
}

void test(char *regex, char *input, char *exp) {
  char *begin = cpsre_match_begin(regex, input);
  char *end = cpsre_match_end(
      regex, begin == NULL || begin == CPSRE_SYNTAX ? input : begin);

  if ((begin == CPSRE_SYNTAX) != (end == CPSRE_SYNTAX))
    abort();
  if ((begin == CPSRE_SYNTAX) != (exp == CPSRE_SYNTAX))
    goto test_failed;
  if (begin == CPSRE_SYNTAX || exp == CPSRE_SYNTAX)
    return;

  if ((begin == NULL) != (end == NULL))
    abort();
  if ((begin == NULL) != (exp == NULL))
    goto test_failed;
  if (begin == NULL || exp == NULL)
    return;

  if (strncmp(begin, exp, end - begin) != 0 || exp[end - begin] != '\0')
    goto test_failed;

  return;

test_failed:
  printf("test failed: "), dump(regex, NULL, '/'), printf(" ");
  printf("against "), dump(input, NULL, '\''), printf(": ");
  printf("expected "), dump(exp, NULL, '\''), printf(", ");
  printf("got "), dump(begin, end, '\''), printf("\n");
}

int main(void) {
  test("a*b+bc", "abbbbc", "abbbbc");
  test("zzz|b+c", "abbbbc", "bbbbc");
  test("zzz|ab+c", "abbbbc", "abbbbc");
  test("a+b|cd", "abbbbc", "ab");
  test("ab+|cd", "abbbbc", "abbbb");
  test("\n", "\n", "\n");
  test("a-z+", "abbbbc", "abbbbc");
  test("a-z+", "abBBbc", "ab");
  test("\\.-4+", "X./01234X", "./01234");
  test("5-\\?+", "X56789:;<=>?X", "56789:;<=>?");
  test("\\(-\\++", "X()*+X", "()*+");
  test("\t-\r+", "X\t\n\v\f\rX", "\t\n\v\f\r");
  test(".", "abc", "a");

  // potential edge cases (mostly from LTRE)
  test("abba", "abba", "abba");
  test("(a|b)+", "abba", "abba");
  test("(a|b)+", "abc", "ab");
  test(".*", "abba", "abba");
  test("\x61\\+", "a+", "a+");
  test("", "", "");
  test("^.", "", NULL);
  test("^.*", "", "");
  test("^.+", "", NULL);
  test("^.?", "", "");
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

  // parse errors (mostly from LTRE)
  test("abc)", "", CPSRE_SYNTAX);
  test("(abc", "", CPSRE_SYNTAX);
  test("+a", "", CPSRE_SYNTAX);
  test("a|*", "", CPSRE_SYNTAX);
  test("\\x0", "", CPSRE_SYNTAX);
  test("\\zzz", "", CPSRE_SYNTAX);
  test("\\b", "", CPSRE_SYNTAX);
  test("\\t", "", CPSRE_SYNTAX);
  test("^^a", "", CPSRE_SYNTAX);
  test("a**", "", CPSRE_SYNTAX);
  test("a+*", "", CPSRE_SYNTAX);
  test("a?*", "", CPSRE_SYNTAX);

  // nonstandard features (mostly from LTRE)
  test("^a", "z", "z");
  test("^a", "a", NULL);
  test("^\n", "\r", "\r");
  test("^\n", "\n", NULL);
  test("^.", "\n", NULL);
  test("^.", "a", NULL);
  test("^a-z*", "1A!2$B", "1A!2$B");
  test("^a-z*", "1aA", "1");
  test("a-z*", "abc", "abc");
  test("\\^", "^", "^");
  test("^\\^", "^", NULL);
  test(".", " ", " ");
  test("^.", " ", NULL);
  test("9-0*", "abc", "abc");
  test("9-0*", "18", "");
  test("9-0*", "09", "09");
  test("9-0*", "/:", "/:");
  test("b-a*", "ab", "ab");
  test("a-b*", "ab", "ab");
  test("a-a*", "ab", "a");
  test("a-a*", "aa", "aa");
  test("-", "", CPSRE_SYNTAX);
  test("a-", "", CPSRE_SYNTAX);

  // realistic regexes (mostly from LTRE)
#define HEX "(0-9|a-f|A-F)"
#define JSON_STR                                                               \
  "\"( -!|#-[|]-\xff|\\\\(\"|\\\\|/|b|f|n|r|t)|\\\\u" HEX HEX HEX HEX ")*\""
  test(JSON_STR, "foo", NULL);
  test(JSON_STR, "\"foo", NULL);
  test(JSON_STR, "foo \"bar\"", "\"bar\"");
  test(JSON_STR, "\"foo\\\"", NULL);
  test(JSON_STR, "\"\\\"", NULL);
  test(JSON_STR, "\"\"\"", "\"\"");
  test(JSON_STR, "\"\"", "\"\"");
  test(JSON_STR, "\"foo\"", "\"foo\"");
  test(JSON_STR, "\"foo\\\"\"", "\"foo\\\"\"");
  test(JSON_STR, "\"foo\\\\\"", "\"foo\\\\\"");
  test(JSON_STR, "\"\\nbar\"", "\"\\nbar\"");
  test(JSON_STR, "\"\nbar\"", NULL);
  test(JSON_STR, "\"\\abar\"", NULL);
  test(JSON_STR, "\"foo\\v\"", NULL);
  test(JSON_STR, "\"\\u1A2b\"", "\"\\u1A2b\"");
  test(JSON_STR, "\"\\uDEAD\"", "\"\\uDEAD\"");
  test(JSON_STR, "\"\\uF00\"", NULL);
  test(JSON_STR, "\"\\uF00BAR\"", "\"\\uF00BAR\"");
  test(JSON_STR, "\"foo\\/\"", "\"foo\\/\"");
  test(JSON_STR, "\"\xcf\x84\"", "\"\xcf\x84\"");
  test(JSON_STR, "\"\x80\"", "\"\x80\"");
  test(JSON_STR, "\"\x88x/\"", "\"\x88x/\"");
  // RFC 8259, $6 'Numbers'
#define JSON_NUM "\\-?(0|1-90-9*)(\\.0-9+)?((e|E)(\\+|\\-)?0-9+)?"
  test(JSON_NUM, "e", NULL);
  test(JSON_NUM, "1", "1");
  test(JSON_NUM, "10", "10");
  test(JSON_NUM, "01", "0");
  test(JSON_NUM, "-5", "-5");
  test(JSON_NUM, "+5", "5");
  test(JSON_NUM, ".3", "3");
  test(JSON_NUM, "2.", "2");
  test(JSON_NUM, "2.3", "2.3");
  test(JSON_NUM, "1e", "1");
  test(JSON_NUM, "1e0", "1e0");
  test(JSON_NUM, "1E+0", "1E+0");
  test(JSON_NUM, "1e-0", "1e-0");
  test(JSON_NUM, "1E10", "1E10");
  test(JSON_NUM, "1e+00", "1e+00");
#define JSON_BOOL "true|false"
#define JSON_NULL "null"
#define JSON_PRIM JSON_STR "|" JSON_NUM "|" JSON_BOOL "|" JSON_NULL
  test(JSON_PRIM, "nul", NULL);
  test(JSON_PRIM, "null", "null");
  test(JSON_PRIM, "nulll", "null");
  test(JSON_PRIM, "true", "true");
  test(JSON_PRIM, "false", "false");
  test(JSON_PRIM, "{}", NULL);
  test(JSON_PRIM, "[]", NULL);
  test(JSON_PRIM, "1,", "1");
  test(JSON_PRIM, "-5.6e2", "-5.6e2");
  test(JSON_PRIM, "\"1a\\n\"", "\"1a\\n\"");
  test(JSON_PRIM, "\"1a\\n\" ", "\"1a\\n\"");
#define FIELD_WIDTH "(\\*|1-90-9*)?"
#define PRECISION "(\\.|\\.\\*|\\.1-90-9*)?"
#define DI "(\\-|\\+| |0)*" FIELD_WIDTH PRECISION "(h|l|j|z|t|hh|ll)?(d|i)"
#define U "(\\-|0)*" FIELD_WIDTH PRECISION "(h|l|j|z|t|hh|ll)?u"
#define OX "(\\-|#|0)*" FIELD_WIDTH PRECISION "(h|l|j|z|t|hh|ll)?(o|x|X)"
#define FEGA "(\\-|\\+| |#|0)*" FIELD_WIDTH PRECISION "(l|L)?(f|F|e|E|g|G|a|A)"
#define C "\\-*" FIELD_WIDTH "l?c"
#define S "\\-*" FIELD_WIDTH PRECISION "l?s"
#define P "\\-*" FIELD_WIDTH "p"
#define N FIELD_WIDTH "(h|l|j|z|t|hh|ll)?n"
#define CONV_SPEC "%(" DI "|" U "|" OX "|" FEGA "|" C "|" S "|" P "|" N "|%)"
  test(CONV_SPEC, "%", NULL);
  test(CONV_SPEC, "%*", NULL);
  test(CONV_SPEC, "%%", "%%");
  test(CONV_SPEC, "%5%", NULL);
  test(CONV_SPEC, "%p", "%p");
  test(CONV_SPEC, "%*p", "%*p");
  test(CONV_SPEC, "% *p", NULL);
  test(CONV_SPEC, "%5p", "%5p");
  test(CONV_SPEC, "d", NULL);
  test(CONV_SPEC, "%d", "%d");
  test(CONV_SPEC, "%.16s", "%.16s");
  test(CONV_SPEC, "% 5.3f", "% 5.3f");
  test(CONV_SPEC, "%*32.4g", NULL);
  test(CONV_SPEC, "%-#65.4g", "%-#65.4g");
  test(CONV_SPEC, "%03c", NULL);
  test(CONV_SPEC, "%06i", "%06i");
  test(CONV_SPEC, "%lu", "%lu");
  test(CONV_SPEC, "%hhu", "%hhu");
  test(CONV_SPEC, "%Lu", NULL);
  test(CONV_SPEC, "%-*p", "%-*p");
  test(CONV_SPEC, "%-.*p", NULL);
  test(CONV_SPEC, "%id", "%i");
  test(CONV_SPEC, "%%d", "%%");
  test(CONV_SPEC, "i%d", "%d");
  test(CONV_SPEC, "%c%s", "%c");
  test(CONV_SPEC, "%0n", NULL);
  test(CONV_SPEC, "% u", NULL);
  test(CONV_SPEC, "%+c", NULL);
  test(CONV_SPEC, "%0-++ 0i", "%0-++ 0i");
  test(CONV_SPEC, "%30c", "%30c");
  test(CONV_SPEC, "%03c", NULL);
#define PRINTF_FMT "(^%|" CONV_SPEC ")*"
  test(PRINTF_FMT, "%", "");
  test(PRINTF_FMT, "%*", "");
  test(PRINTF_FMT, "%%", "%%");
  test(PRINTF_FMT, "%5%", "");
  test(PRINTF_FMT, "%id", "%id");
  test(PRINTF_FMT, "%%d", "%%d");
  test(PRINTF_FMT, "i%d", "i%d");
  test(PRINTF_FMT, "%c%s", "%c%s");
  test(PRINTF_FMT, "%u + %d", "%u + %d");
  test(PRINTF_FMT, "%d:", "%d:");
#define UTF8_CHAR                                                              \
  "(\x01-\x7f|(\xc2-\xdf|\xe0\xa0-\xbf|\xed\x80-\x9f|(\xe1-\xec|\xee|\xef|"    \
  "\xf0\x90-\xbf|\xf4\x80-\x8f|\xf1-\xf3\x80-\xbf)\x80-\xbf)\x80-\xbf)"
#define UTF8_CHARS UTF8_CHAR "*"
  test(UTF8_CHAR, "ab", "a");
  test(UTF8_CHAR, "\x80x", "x");
  test(UTF8_CHAR, "\x80", NULL);
  test(UTF8_CHAR, "\xbf", NULL);
  test(UTF8_CHAR, "\xc0", NULL);
  test(UTF8_CHAR, "\xc1", NULL);
  test(UTF8_CHAR, "\xff", NULL);
  test(UTF8_CHAR, "\xed\xa1\x8c", NULL); // RFC ex. (surrogate)
  test(UTF8_CHAR, "\xed\xbe\xb4", NULL); // RFC ex. (surrogate)
  test(UTF8_CHAR, "\xed\xa0\x80", NULL); // d800, first surrogate
  test(UTF8_CHAR, "\xc0\x80", NULL);     // RFC ex. (overlong nul)
  test(UTF8_CHAR, "\x7f", "\x7f");
  test(UTF8_CHAR, "\xf0\x9e\x84\x93", "\xf0\x9e\x84\x93");
  test(UTF8_CHAR, "\x2f", "\x2f");           // solidus
  test(UTF8_CHAR, "\xc0\xaf", NULL);         // overlong solidus
  test(UTF8_CHAR, "\xe0\x80\xaf", NULL);     // overlong solidus
  test(UTF8_CHAR, "\xf0\x80\x80\xaf", NULL); // overlong solidus
  test(UTF8_CHAR, "\xf7\xbf\xbf\xbf", NULL); // 1fffff, too big
  test(UTF8_CHARS, "\x41\xe2\x89\xa2\xce\x91\x2e",
       "\x41\xe2\x89\xa2\xce\x91\x2e"); // RFC ex.
  test(UTF8_CHARS, "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4",
       "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4"); // RFC ex.
  test(UTF8_CHARS, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
       "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"); // RFC ex.
  test(UTF8_CHARS, "\xef\xbb\xbf\xf0\xa3\x8e\xb4",
       "\xef\xbb\xbf\xf0\xa3\x8e\xb4"); // RFC ex.
  test(UTF8_CHARS, "abcABC123<=>", "abcABC123<=>");
  test(UTF8_CHARS, "\xc2\x80", "\xc2\x80");
  test(UTF8_CHARS, "\xc2\x7f", "");     // bad tail
  test(UTF8_CHARS, "\xe2\x28\xa1", ""); // bad tail
  test(UTF8_CHARS, "\x80x/", "");
}
