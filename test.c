#include "cps-re.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dump(char *begin, char *end, char delim) {
  if (begin == NULL)
    printf("no match");
  else {
    putchar(delim);
    for (; *begin && (end == NULL || begin < end); begin++)
      printf(isprint(*begin) && *begin != '\\' ? "%c" : "\\x%02hhx", *begin);
    putchar(delim);
  }
}

void test(char *regex, char *input, char *partial, bool exact) {
  // run `regex` against `input` and ensure that the first partial match is
  // `partial` and that an exact match is possible if and only if `exact`.
  // also ensure that `regex` fails to parse if and only if `input == NULL`

  bool parse_error = *cpsre_parse(regex) != '\0';
  if (parse_error != (input == NULL))
    printf("test failed: "), dump(regex, NULL, '/'), printf(" parse\n");
  if (parse_error || input == NULL)
    return;

  char *partial_begin = cpsre_unanchored(regex, input, NULL);
  char *partial_end = cpsre_anchored(
      regex, partial_begin == NULL ? input : partial_begin, NULL);
  char *exact_end = cpsre_anchored(regex, input, strchr(input, '\0'));

  if (exact_end != NULL && exact_end != strchr(input, '\0'))
    abort();
  if (!!exact_end != exact) {
    printf("test failed: "), dump(regex, NULL, '/'), printf(" ");
    printf("against "), dump(input, NULL, '\''), printf(": ");
    printf("expected "), printf(exact ? "exact match\n" : "no exact match\n");
    return;
  }

  if ((partial_begin == NULL) != (partial_end == NULL))
    abort();
  if ((partial_begin == NULL) != (partial == NULL))
    goto partial_failed;
  if (partial_begin == NULL || partial == NULL)
    return;

  if (strncmp(partial_begin, partial, partial_end - partial_begin) != 0 ||
      partial[partial_end - partial_begin] != '\0') {
  partial_failed:
    printf("test failed: "), dump(regex, NULL, '/'), printf(" ");
    printf("against "), dump(input, NULL, '\''), printf(": ");
    printf("expected "), dump(partial, NULL, '\''), printf(", ");
    printf("got "), dump(partial_begin, partial_end, '\''), printf("\n");
    return;
  }
}

int main(void) {
  // potential edge cases (mostly from LTRE)
  test("abba", "abba", "abba", true);
  test("ab|abba", "abba", "ab", true);
  test("(a|b)+", "abba", "abba", true);
  test("(a|b)+", "abc", "ab", false);
  test(".", "abba", "a", false);
  test(".*", "abba", "abba", true);
  test("\x61\\+", "a+", "a+", true);
  test("a*b+bc", "abbbbc", "abbbbc", true);
  test("zzz|b+c", "abbbbc", "bbbbc", false);
  test("zzz|ab+c", "abbbbc", "abbbbc", true);
  test("a+b|c", "abbbbc", "ab", false);
  test("ab+|c", "abbbbc", "abbbb", false);
  test("", "", "", true);
  test("~.", "", NULL, false);
  test("~.*", "", "", true);
  test("~.+", "", NULL, false);
  test("~.?", "", "", true);
  test("()", "", "", true);
  test("()*", "", "", true);
  test("()+", "", "", true);
  test("()?", "", "", true);
  test("(())", "", "", true);
  test("()()", "", "", true);
  test("()", "a", "", false);
  test("a()", "a", "a", true);
  test("()a", "a", "a", true);
  test("", "\n", "", false);
  test("\n", "\n", "\n", true);
  test(".", "\n", "\n", true);
  test("\\\\n", "\n", NULL, false);
  test("(|n)(\n)", "\n", "\n", true);
  test("\r?\n", "\n", "\n", true);
  test("\r?\n", "\r\n", "\r\n", true);
  test("(a*)*", "a", "a", true);
  test("(a+)+", "aa", "aa", true);
  test("(a?)?", "", "", true);
  test("a+", "aa", "aa", true);
  test("a?", "aa", "a", false);
  test("(a+)?", "aa", "aa", true);
  test("(ba+)?", "baa", "baa", true);
  test("(ab+)?", "b", "", false);
  test("(a+b)?", "a", "", false);
  test("(a+a+)+", "a", NULL, false);
  test("a+", "", NULL, false);
  test("(a+|)+", "aa", "aa", true);
  test("(a+|)+", "", "", true);
  test("(a|b)?", "", "", true);
  test("(a|b)?", "a", "a", true);
  test("(a|b)?", "b", "b", true);
  test("x*|", "xx", "xx", true);
  test("x*|", "", "", true);
  test("x+|", "xx", "xx", true);
  test("x+|", "", "", true);
  test("x?|", "x", "x", true);
  test("x?|", "", "", true);
  test("x*y*", "yx", "y", false);
  test("x+y+", "yx", NULL, false);
  test("x?y?", "yx", "y", false);
  test("x+y*", "xyx", "xy", false);
  test("x*y+", "yxy", "y", false);
  test("x*|y*", "xy", "x", false);
  test("x+|y+", "xy", "x", false);
  test("x?|y?", "xy", "x", false);
  test("x+|y*", "xy", "x", false);
  test("x*|y+", "xy", "x", false);

  // greedy, lazy, possessive
  test("a*", "aa", "aa", true);
  test("a+", "aa", "aa", true);
  test("a?", "a", "a", true);
  test("a*a", "aa", "aa", true);
  test("a+a", "aa", "aa", true);
  test("a?a", "a", "a", true);
  test("a*?", "aa", "", true);
  test("a+?", "aa", "a", true);
  test("a??", "a", "", true);
  test("a*?a", "aa", "a", true);
  test("a+?a", "aa", "aa", true);
  test("a??a", "aa", "a", true);
  test("a*+", "aa", "aa", true);
  test("a++", "aa", "aa", true);
  test("a?+", "a", "a", true);
  test("a*+a", "aa", NULL, false);
  test("a++a", "aa", NULL, false);
  test("a?+a", "a", NULL, false);
  test("(a?+)++a", "aaa", NULL, false);
  test("(a++)?+a", "aaa", NULL, false);
  test("(aa?+)?+a", "a", NULL, false);
  test("(aa++)++a", "a", NULL, false);
  test("!(!a*)", "aa", "", true);
  test("!(!a+)", "aa", "a", true);
  test("!(!a?)", "a", "", true);
  test("!(!a*a)", "aa", "a", true);
  test("!(!a+a)", "aa", "aa", true);
  test("!(!a?a)", "a", "a", true);
  test("%", "aa", "", true);
  test("%*", "aa", "aa", true);
  test("%+", "aa", "aa", true);
  test("%?", "a", "", true);
  test("%a", "aa", "a", true);
  test("%*a", "aa", "aa", true);
  test("%+a", "aa", "aa", true);
  test("%?a", "a", "a", true);

  // parse errors (mostly from LTRE)
  test("abc)", NULL, NULL, false);
  test("(abc", NULL, NULL, false);
  test("+a", NULL, NULL, false);
  test("a|*", NULL, NULL, false);
  test("\\", NULL, NULL, false);
  test("\\x0", NULL, NULL, false);
  test("\\yyy", NULL, NULL, false);
  test("\\a", NULL, NULL, false);
  test("\\b", NULL, NULL, false);
  test("~~a", NULL, NULL, false);
  test("a**", NULL, NULL, false);
  test("a+*", NULL, NULL, false);
  test("a?*", NULL, NULL, false);

  // nonstandard features (mostly from LTRE)
  test("~a", "z", "z", true);
  test("~a", "a", NULL, false);
  test("~\n", "\r", "\r", true);
  test("~\n", "\n", NULL, false);
  test("~.", "\n", NULL, false);
  test("~.", "a", NULL, false);
  test("~a-z*", "1A!2$B", "1A!2$B", true);
  test("~a-z*", "1aA", "1", false);
  test("a-z*", "abc", "abc", true);
  test("\\~", "~", "~", true);
  test("~\\~", "~", NULL, false);
  test("9-0*", "abc", "abc", true);
  test("9-0*", "18", "", false);
  test("9-0*", "09", "09", true);
  test("9-0*", "/:", "/:", true);
  test("b-a*", "ab", "ab", true);
  test("a-b*", "ab", "ab", true);
  test("a-a*", "ab", "a", false);
  test("a-a*", "aa", "aa", true);
  test("\\.-4+", "./01234", "./01234", true);
  test("5-\\?+", "56789:;<=>?", "56789:;<=>?", true);
  test("\\(-\\++", "()*+", "()*+", true);
  test("\t-\r+", "\t\n\v\f\r", "\t\n\v\f\r", true);
  test("!", "", NULL, false);
  test("!", "a", "a", true);
  test("!", "aa", "a", true);
  test("!0*?", "", NULL, false);
  test("!0*?", "0", NULL, false);
  test("!0*?", "00", NULL, false);
  test("!0*?", "001", "001", true);
  test("ab&cd", "", NULL, false);
  test("ab&cd", "ab", NULL, false);
  test("ab&cd", "cd", NULL, false);
  test("...&%?a%?", "ab", NULL, false);
  test("...&%?a%?", "abc", "abc", true);
  test("...&%?a%?", "bcd", NULL, false);
  test("a&b|c", "a", NULL, false);
  test("a&b|c", "b", NULL, false);
  test("a&b|c", "c", NULL, false);
  test("a|b&c", "a", "a", true);
  test("a|b&c", "b", NULL, false);
  test("a|b&c", "c", NULL, false);
  test("(0-9|a-z|A-Z)+&!0-9+?", "", NULL, false);
  test("(0-9|a-z|A-Z)+&!0-9+?", "abc", "abc", true);
  test("(0-9|a-z|A-Z)+&!0-9+?", "abc123", "abc123", true);
  test("(0-9|a-z|A-Z)+&!0-9+?", "1a2b3c", "1a2b3c", true);
  test("(0-9|a-z|A-Z)+&!0-9+?", "123", NULL, false);
  test("0x(!(0-9|a-f|A-F)+?)", "0yz", NULL, false);
  test("0x(!(0-9|a-f|A-F)+?)", "0x12", "0x", false);
  test("0x(!(0-9|a-f|A-F)+?)", "0x", "0x", true);
  test("0x(!(0-9|a-f|A-F)+?)", "0xy", "0x", true);
  test("0x(!(0-9|a-f|A-F)+?)", "0xyz", "0x", true);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0yz", NULL, false);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0x12", "0x", false);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0x", "0x", true);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0xy", "0xy", true);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0xyz", "0xy", true);
  test("!(!)(!a)", "", "", true);
  test("!(!)(!a)", "a", "", false);
  test("!(!)(!a)", "ab", "", false);
  test("!(!a)(!)", "", "", true);
  test("!(!a)(!)", "a", "", false);
  test("!(!a)(!)", "ab", "", false);
  test("!(!)(!)", "", "", true);
  test("!(!)(!)", "a", "", true);
  test("!(!)(!)", "ab", "", false);
  test("!(!)/(!0*?)", "/2", "", true);
  test("!(!)/(!0*?)", "1/0", "", true);
  test("!(!)/(!0*?)", "1/2", "", false);
  test("!(!)/(!0*?)", "1/", "", true);
  test("!(!)/(!0*?)", "2/1/0", "", false);
  test("!(!)/(!0*?)", "1/00", "", true);
  test("!(!)/(!0*?)", "0/2", "", false);
  test("!(!)/(!0*?)", "a/b", "", false);
  test("!(!)/(!0*?)", "a-b", "", true);
  test("!(!a-z*?0-9*?)?", "", NULL, false);
  test("!(!a-z*?0-9*?)?", "b", "b", true);
  test("!(!a-z*?0-9*?)?", "2", "2", true);
  test("!(!a-z*?0-9*?)?", "c3", "c", true);
  test("!(!a-z*?0-9*?)?", "4d", "4", false);
  test("!(!a-z*?0-9*?)?", "ee56", "e", true);
  test("!(!a-z*?0-9*?)?", "f6g", "f", false);
  test("b(!a*?)", "", NULL, false);
  test("b(!a*?)", "b", NULL, false);
  test("b(!a*?)", "ba", NULL, false);
  test("b(!a*?)", "bbaa", "bb", true);
  test("a*(!)", "", NULL, false);
  test("a*(!)", "a", "a", true);
  test("a*(!)", "bc", "b", true);
  test("(!)*", "", "", true);
  test("(!)*", "a", "a", true);
  test("(!)*", "ab", "ab", true);
  test("(!)+", "", NULL, false);
  test("(!)+", "a", "a", true);
  test("(!)+", "ab", "ab", true);
  test("(!)?", "", "", true);
  test("(!)?", "a", "a", true);
  test("(!)?", "ab", "a", true);
  test("(a*)*", "a", "a", true);
  test("(a*)+", "a", "a", true);
  test("(a*)?", "a", "a", true);
  test("(a+)*", "a", "a", true);
  test("(a+)+", "a", "a", true);
  test("(a+)?", "a", "a", true);
  test("(a?)*", "a", "a", true);
  test("(a?)+", "a", "a", true);
  test("(a?)?", "a", "a", true);
  test("-", NULL, NULL, false);
  test("--", NULL, NULL, false);
  test("---", NULL, NULL, false);
  test("a-", NULL, NULL, false);
  test(".-a", NULL, NULL, false);
  test("a-.", NULL, NULL, false);
  test("a-\\", NULL, NULL, false);
  test("!!a", NULL, NULL, false);
  test("a!!b", NULL, NULL, false);

  // realistic regexes (mostly from LTRE)
#define HEX_RGB "#(...(...)?&(0-9|a-f|A-F)*?)"
  test(HEX_RGB, "000", NULL, false);
  test(HEX_RGB, "#0aA", "#0aA", true);
  test(HEX_RGB, "#00ff", "#00f", false);
  test(HEX_RGB, "#abcdef", "#abcdef", true);
  test(HEX_RGB, "#abcdeff", "#abcdef", false);
#define BLOCK_COMMENT "/\\*(!%\\*/%)\\*/"
#define LINE_COMMENT "//~\n*\n"
#define COMMENT BLOCK_COMMENT "|" LINE_COMMENT
  test(COMMENT, "// */\n", "// */\n", true);
  test(COMMENT, "// //\n", "// //\n", true);
  test(COMMENT, "/* */", "/* */", true);
  test(COMMENT, "/*/", NULL, false);
  test(COMMENT, "/*/*/", "/*/*/", true);
  test(COMMENT, "/**/*/", "/**/", false);
  test(COMMENT, "/*/**/*/", "/*/**/", false);
  test(COMMENT, "/*//*/", "/*//*/", true);
  test(COMMENT, "/**/\n", "/**/", false);
  test(COMMENT, "//**/\n", "//**/\n", true);
  test(COMMENT, "///*\n*/", "///*\n", false);
  test(COMMENT, "//\n\n", "//\n", false);
#define FIELD_WIDTH "(\\*|1-90-9*)?"
#define PRECISION "(\\.(\\*|1-90-9*)?)?"
#define DI "(\\-|\\+| |0)*" FIELD_WIDTH PRECISION "(hh|ll|h|l|j|z|t)?(d|i)"
#define U "(\\-|0)*" FIELD_WIDTH PRECISION "(hh|ll|h|l|j|z|t)?u"
#define OX "(\\-|#|0)*" FIELD_WIDTH PRECISION "(hh|ll|h|l|j|z|t)?(o|x|X)"
#define FEGA "(\\-|\\+| |#|0)*" FIELD_WIDTH PRECISION "(l|L)?(f|F|e|E|g|G|a|A)"
#define C "\\-*" FIELD_WIDTH "l?c"
#define S "\\-*" FIELD_WIDTH PRECISION "l?s"
#define P "\\-*" FIELD_WIDTH "p"
#define N FIELD_WIDTH "(hh|ll|h|l|j|z|t)?n"
#define CONV_SPEC                                                              \
  "\\%(" DI "|" U "|" OX "|" FEGA "|" C "|" S "|" P "|" N "|\\%)"
  test(CONV_SPEC, "%", NULL, false);
  test(CONV_SPEC, "%*", NULL, false);
  test(CONV_SPEC, "%%", "%%", true);
  test(CONV_SPEC, "%5%", NULL, false);
  test(CONV_SPEC, "%p", "%p", true);
  test(CONV_SPEC, "%*p", "%*p", true);
  test(CONV_SPEC, "% *p", NULL, false);
  test(CONV_SPEC, "%5p", "%5p", true);
  test(CONV_SPEC, "d", NULL, false);
  test(CONV_SPEC, "%d", "%d", true);
  test(CONV_SPEC, "%*d", "%*d", true);
  test(CONV_SPEC, "%**d", NULL, false);
  test(CONV_SPEC, "%.16s", "%.16s", true);
  test(CONV_SPEC, "% 5.3f", "% 5.3f", true);
  test(CONV_SPEC, "%*32.4g", NULL, false);
  test(CONV_SPEC, "%-#65.4g", "%-#65.4g", true);
  test(CONV_SPEC, "%03c", NULL, false);
  test(CONV_SPEC, "%06i", "%06i", true);
  test(CONV_SPEC, "%lu", "%lu", true);
  test(CONV_SPEC, "%hhu", "%hhu", true);
  test(CONV_SPEC, "%Lu", NULL, false);
  test(CONV_SPEC, "%-*p", "%-*p", true);
  test(CONV_SPEC, "%-.*p", NULL, false);
  test(CONV_SPEC, "%id", "%i", false);
  test(CONV_SPEC, "%%d", "%%", false);
  test(CONV_SPEC, "i%d", "%d", false);
  test(CONV_SPEC, "%c%s", "%c", false);
  test(CONV_SPEC, "%0n", NULL, false);
  test(CONV_SPEC, "% u", NULL, false);
  test(CONV_SPEC, "%+c", NULL, false);
  test(CONV_SPEC, "%0-++ 0i", "%0-++ 0i", true);
  test(CONV_SPEC, "%30c", "%30c", true);
  test(CONV_SPEC, "%03c", NULL, false);
#define PRINTF_FMT "(~\\%|" CONV_SPEC ")*+"
  test(PRINTF_FMT, "%", "", false);
  test(PRINTF_FMT, "%*", "", false);
  test(PRINTF_FMT, "%%", "%%", true);
  test(PRINTF_FMT, "%5%", "", false);
  test(PRINTF_FMT, "%id", "%id", true);
  test(PRINTF_FMT, "%%d", "%%d", true);
  test(PRINTF_FMT, "i%d", "i%d", true);
  test(PRINTF_FMT, "%c%s", "%c%s", true);
  test(PRINTF_FMT, "%u + %d", "%u + %d", true);
  test(PRINTF_FMT, "%d:", "%d:", true);
#define KEYWORD                                                                \
  "(auto|break|case|char|const|continue|default|do|double|else|enum|extern|"   \
  "float|for|goto|if|inline|int|long|register|restrict|return|short|signed|"   \
  "sizeof|static|struct|switch|typedef|union|unsigned|void|volatile|while|"    \
  "_Bool|_Complex|_Imaginary)"
#define IDENTIFIER                                                             \
  "(_|0-9|a-z|A-Z|\\\\u(....&(0-9|a-f|A-F)*?)|\\\\U(........&(0-9|a-f|A-F)*?)" \
  ")++&!0-9%&!" KEYWORD
  test(IDENTIFIER, "", NULL, false);
  test(IDENTIFIER, "_", "_", true);
  test(IDENTIFIER, "_foo", "_foo", true);
  test(IDENTIFIER, "_Bool", "Bool", false);
  test(IDENTIFIER, "a1", "a1", true);
  test(IDENTIFIER, "5b", "b", false);
  test(IDENTIFIER, "if", "f", false);
  test(IDENTIFIER, "ifa", "ifa", true);
  test(IDENTIFIER, "bif", "bif", true);
  test(IDENTIFIER, "if2", "if2", true);
  test(IDENTIFIER, "1if", "f", false);
  test(IDENTIFIER, "\\u12", "u12", false);
  test(IDENTIFIER, "\\u1A2b", "\\u1A2b", true);
  test(IDENTIFIER, "\\u1234", "\\u1234", true);
  test(IDENTIFIER, "\\u123x", "u123x", false);
  test(IDENTIFIER, "\\u1234x", "\\u1234x", true);
  test(IDENTIFIER, "\\U12345678", "\\U12345678", true);
  test(IDENTIFIER, "\\U1234567y", "U1234567y", false);
  test(IDENTIFIER, "\\U12345678y", "\\U12345678y", true);
#define JSON_STR                                                               \
  "\"(( -\xff&~\"&~\\\\)|\\\\(\"|\\\\|/|b|f|n|r|t)|"                           \
  "\\\\u(....&(0-9|a-f|A-F)*?))*+\""
  test(JSON_STR, "foo", NULL, false);
  test(JSON_STR, "\"foo", NULL, false);
  test(JSON_STR, "foo \"bar\"", "\"bar\"", false);
  test(JSON_STR, "\"foo\\\"", NULL, false);
  test(JSON_STR, "\"\\\"", NULL, false);
  test(JSON_STR, "\"\"\"", "\"\"", false);
  test(JSON_STR, "\"\"", "\"\"", true);
  test(JSON_STR, "\"foo\"", "\"foo\"", true);
  test(JSON_STR, "\"foo\\\"\"", "\"foo\\\"\"", true);
  test(JSON_STR, "\"foo\\\\\"", "\"foo\\\\\"", true);
  test(JSON_STR, "\"\\nbar\"", "\"\\nbar\"", true);
  test(JSON_STR, "\"\nbar\"", NULL, false);
  test(JSON_STR, "\"\\abar\"", NULL, false);
  test(JSON_STR, "\"foo\\v\"", NULL, false);
  test(JSON_STR, "\"\\u1A2b\"", "\"\\u1A2b\"", true);
  test(JSON_STR, "\"\\uDEAD\"", "\"\\uDEAD\"", true);
  test(JSON_STR, "\"\\uF00\"", NULL, false);
  test(JSON_STR, "\"\\uF00BAR\"", "\"\\uF00BAR\"", true);
  test(JSON_STR, "\"foo\\/\"", "\"foo\\/\"", true);
  test(JSON_STR, "\"\xcf\x84\"", "\"\xcf\x84\"", true);
  test(JSON_STR, "\"\x80\"", "\"\x80\"", true);
  test(JSON_STR, "\"\x88x/\"", "\"\x88x/\"", true);
#define JSON_NUM "\\-?(0|1-90-9*)(\\.0-9+)?((e|E)(\\+|\\-)?0-9+)?"
  test(JSON_NUM, "e", NULL, false);
  test(JSON_NUM, "1", "1", true);
  test(JSON_NUM, "10", "10", true);
  test(JSON_NUM, "01", "0", false);
  test(JSON_NUM, "-5", "-5", true);
  test(JSON_NUM, "+5", "5", false);
  test(JSON_NUM, ".3", "3", false);
  test(JSON_NUM, "2.", "2", false);
  test(JSON_NUM, "2.3", "2.3", true);
  test(JSON_NUM, "1e", "1", false);
  test(JSON_NUM, "1e0", "1e0", true);
  test(JSON_NUM, "1E+0", "1E+0", true);
  test(JSON_NUM, "1e-0", "1e-0", true);
  test(JSON_NUM, "1E10", "1E10", true);
  test(JSON_NUM, "1e+00", "1e+00", true);
#define JSON_BOOL "true|false"
#define JSON_NULL "null"
#define JSON_PRIM JSON_STR "|" JSON_NUM "|" JSON_BOOL "|" JSON_NULL
  test(JSON_PRIM, "nul", NULL, false);
  test(JSON_PRIM, "null", "null", true);
  test(JSON_PRIM, "nulll", "null", false);
  test(JSON_PRIM, "true", "true", true);
  test(JSON_PRIM, "false", "false", true);
  test(JSON_PRIM, "{}", NULL, false);
  test(JSON_PRIM, "[]", NULL, false);
  test(JSON_PRIM, "1,", "1", false);
  test(JSON_PRIM, "-5.6e2", "-5.6e2", true);
  test(JSON_PRIM, "\"1a\\n\"", "\"1a\\n\"", true);
  test(JSON_PRIM, "\"1a\\n\" ", "\"1a\\n\"", false);
#define UTF8_CHAR                                                              \
  "(~\x80-\xff|(\xc2-\xdf|\xe0\xa0-\xbf|\xed\x80-\x9f|(\xe1-\xec|\xee-\xef|"   \
  "\xf0\x90-\xbf|\xf4\x80-\x8f|\xf1-\xf3\x80-\xbf)\x80-\xbf)\x80-\xbf)"
#define UTF8_CHARS UTF8_CHAR "*+"
  test(UTF8_CHAR, "ab", "a", false);
  test(UTF8_CHAR, "\x80x", "x", false);
  test(UTF8_CHAR, "\x80", NULL, false);
  test(UTF8_CHAR, "\xbf", NULL, false);
  test(UTF8_CHAR, "\xc0", NULL, false);
  test(UTF8_CHAR, "\xc1", NULL, false);
  test(UTF8_CHAR, "\xff", NULL, false);
  test(UTF8_CHAR, "\xed\xa1\x8c", NULL, false); // RFC ex. (surrogate)
  test(UTF8_CHAR, "\xed\xbe\xb4", NULL, false); // RFC ex. (surrogate)
  test(UTF8_CHAR, "\xed\xa0\x80", NULL, false); // d800, first surrogate
  test(UTF8_CHAR, "\xc0\x80", NULL, false);     // RFC ex. (overlong nul)
  test(UTF8_CHAR, "\x7f", "\x7f", true);
  test(UTF8_CHAR, "\xf0\x9e\x84\x93", "\xf0\x9e\x84\x93", true);
  test(UTF8_CHAR, "\x2f", "\x2f", true);            // solidus
  test(UTF8_CHAR, "\xc0\xaf", NULL, false);         // overlong solidus
  test(UTF8_CHAR, "\xe0\x80\xaf", NULL, false);     // overlong solidus
  test(UTF8_CHAR, "\xf0\x80\x80\xaf", NULL, false); // overlong solidus
  test(UTF8_CHAR, "\xf7\xbf\xbf\xbf", NULL, false); // 1fffff, too big
  test(UTF8_CHARS, "\x41\xe2\x89\xa2\xce\x91\x2e",
       "\x41\xe2\x89\xa2\xce\x91\x2e", true); // RFC ex.
  test(UTF8_CHARS, "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4",
       "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4", true); // RFC ex.
  test(UTF8_CHARS, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
       "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", true); // RFC ex.
  test(UTF8_CHARS, "\xef\xbb\xbf\xf0\xa3\x8e\xb4",
       "\xef\xbb\xbf\xf0\xa3\x8e\xb4", true); // RFC ex.
  test(UTF8_CHARS, "abcABC123<=>", "abcABC123<=>", true);
  test(UTF8_CHARS, "\xc2\x80", "\xc2\x80", true);
  test(UTF8_CHARS, "\xc2\x7f", "", false);     // bad tail
  test(UTF8_CHARS, "\xe2\x28\xa1", "", false); // bad tail
  test(UTF8_CHARS, "\x80x/", "", false);
#define OCTET "(250-5|(20-4|10-9|1-9?)0-9)"
#define IPV4 OCTET "\\." OCTET "\\." OCTET "\\." OCTET
  test(IPV4, "0.0.0.0", "0.0.0.0", true);
  test(IPV4, "1.1.1.1", "1.1.1.1", true);
  test(IPV4, "10.0.0.4", "10.0.0.4", true);
  test(IPV4, "127.0.0.1", "127.0.0.1", true);
  test(IPV4, "192.168.0.1", "192.168.0.1", true);
  test(IPV4, "55.148.8.11", "55.148.8.11", true);
  test(IPV4, "255.255.255.255", "255.255.255.255", true);
  test(IPV4, "1..1.1", NULL, false);
  test(IPV4, "0.0.0.0.", "0.0.0.0", false);
  test(IPV4, ".0.0.0.0", "0.0.0.0", false);
  test(IPV4, "1.1.01.1", NULL, false);
  test(IPV4, "10.0.0.256", "10.0.0.25", false);
  test(IPV4, "12.224.29.25.149", "12.224.29.25", false);
#define FA_PATH(TRANS) "A-Z(0-1A-Z)*&!%(A-Z0-1A-Z&!(" TRANS "))%"
#define ACCEPT FA_PATH("A1B|A0D|D0D|D1D|B1B|C1B|B0C|C0C") "&A%%C"
#define REJECT FA_PATH("A1B|A0D|D0D|D1D|B1B|C1B|B0C|C0C") "&A%~C"
  test(ACCEPT, "A0D", NULL, false);
  test(ACCEPT, "A0C", NULL, false);
  test(ACCEPT, "A5B", NULL, false);
  test(ACCEPT, "A1B", NULL, false);
  test(ACCEPT, "A1B0C", "A1B0C", true);
  test(ACCEPT, "A1B1B0C0C", "A1B1B0C0C", true);
  test(ACCEPT, "B1B0C", NULL, false);
  test(REJECT, "A0D", "A0D", true);
  test(REJECT, "A0C", NULL, false);
  test(REJECT, "A5B", NULL, false);
  test(REJECT, "A1B", "A1B", true);
  test(REJECT, "A1B0C", "A1B", false);
  test(REJECT, "A1B1B0C0C", "A1B1B", false);
  test(REJECT, "B1B0C", NULL, false);
#define YEAR "(0-90-90-90-9|(0|5-9)0-9)"
#define MONTH "(0?1-9|10-2)"
#define UNAMBIGUOUS                                                            \
  "(" YEAR "/" MONTH "|" MONTH "/" YEAR ")&!" YEAR "/" MONTH "|!" MONTH "/" YEAR
  test(UNAMBIGUOUS, "3/98", "3/98", true);
  test(UNAMBIGUOUS, "05/98", "05/9", true);
  test(UNAMBIGUOUS, "10/98", "10/98", true);
  test(UNAMBIGUOUS, "98/12", "98/1", true);
  test(UNAMBIGUOUS, "98/13", "98/1", false);
  test(UNAMBIGUOUS, "98/17", "98/1", false);
  test(UNAMBIGUOUS, "07/55", "07/5", true);
  test(UNAMBIGUOUS, "07/14", "07/1", false);
  test(UNAMBIGUOUS, "07/1914", "07/1", true);
  test(UNAMBIGUOUS, "07/2014", "07/2", true);
  test(UNAMBIGUOUS, "3/2", NULL, false);
  test(UNAMBIGUOUS, "3/02", "3/02", true);
  test(UNAMBIGUOUS, "03/2", "03/2", true);
  test(UNAMBIGUOUS, "03/02", "3/02", false);
  test(UNAMBIGUOUS, "03/2002", "03/2", true);
  test(UNAMBIGUOUS, "2003/02", "2003/02", true);
  test(UNAMBIGUOUS, "11/12", NULL, false);
  test(UNAMBIGUOUS, "2011/12", "2011/1", true);
  test(UNAMBIGUOUS, "11/2012", "11/2012", true);
#define DIV_BY_3                                                               \
  "(0|3|6|9|(1|4|7)(0|3|6|9)*(2|5|8)|(2|5|8|(1|4|7)(0|3|6|9)*(1|4|7))"         \
  "(0|3|6|9|(2|5|8)(0|3|6|9)*(1|4|7))*(1|4|7|(2|5|8)(0|3|6|9)*(2|5|8)))*"
  test(DIV_BY_3, "", "", true);
  test(DIV_BY_3, "3", "3", true);
  test(DIV_BY_3, "4818", "4818", true);
  test(DIV_BY_3, "756", "756", true);
  test(DIV_BY_3, "146", "", false);
  test(DIV_BY_3, "446127512", "44612751", false);
  test(DIV_BY_3, "24641410726", "246414", false);
  test(DIV_BY_3, "6012627460", "6012627", false);
  test(DIV_BY_3, "91564250", "915642", false);
  test(DIV_BY_3, "2308562", "230856", false);
  test(DIV_BY_3, "76", "", false);
  test(DIV_BY_3, "2222530", "222", false);
  test(DIV_BY_3, "18", "18", true);
  test(DIV_BY_3, "10361335", "", false);
  test(DIV_BY_3, "1374", "1374", true);
  test(DIV_BY_3, "70", "", false);
  test(DIV_BY_3, "26054309489", "260543094", false);
  test(DIV_BY_3, "124859573097", "124859573097", true);
#define PWD_REQ "........+& -\\~*&%a-z%&%A-Z%&%0-9%&%(\\!-/|:-@|[-`|{-\\~)%"
  test(PWD_REQ, "pa$$W0rd", "pa$$W0rd", true);
  test(PWD_REQ, "Password1!", "Password1!", true);
  test(PWD_REQ, "Password1", NULL, false);
  test(PWD_REQ, "password1!", NULL, false);
  test(PWD_REQ, "PASSWORD1!", NULL, false);
  test(PWD_REQ, "Password!", NULL, false);
  test(PWD_REQ, "Pass1!", NULL, false);
  test(PWD_REQ, "Password\t1!", NULL, false);
#define NUM_ID "(0|1-90-9*+)"
#define PREREL_ID "(" BUILD_ID "&!00-9+?)"
#define BUILD_ID "(0-9|a-z|A-Z|\\-)+"
#define CORE NUM_ID "\\." NUM_ID "\\." NUM_ID
#define PREREL PREREL_ID "(\\." PREREL_ID ")*+"
#define BUILD BUILD_ID "(\\." BUILD_ID ")*+"
#define SEMVER CORE "(\\-" PREREL ")?+(\\+" BUILD ")?+"
  test(SEMVER, "0.0.4", "0.0.4", true);
  test(SEMVER, "1.2.3", "1.2.3", true);
  test(SEMVER, "10.20.30", "10.20.30", true);
  test(SEMVER, "1.1.2-prerelease+meta", "1.1.2-prerelease+meta", true);
  test(SEMVER, "1.1.2+meta", "1.1.2+meta", true);
  test(SEMVER, "1.1.2+meta-valid", "1.1.2+meta-valid", true);
  test(SEMVER, "1.0.0-alpha", "1.0.0-alpha", true);
  test(SEMVER, "1.0.0-beta", "1.0.0-beta", true);
  test(SEMVER, "1.0.0-alpha.beta", "1.0.0-alpha.beta", true);
  test(SEMVER, "1.0.0-alpha.beta.1", "1.0.0-alpha.beta.1", true);
  test(SEMVER, "1.0.0-alpha.1", "1.0.0-alpha.1", true);
  test(SEMVER, "1.0.0-alpha0.valid", "1.0.0-alpha0.valid", true);
  test(SEMVER, "1.0.0-alpha.0valid", "1.0.0-alpha.0valid", true);
  test(SEMVER, "1.0.0-alpha-a.b-c-somethinglong+build.1-aef.1-its-okay",
       "1.0.0-alpha-a.b-c-somethinglong+build.1-aef.1-its-okay", true);
  test(SEMVER, "1.0.0-rc.1+build.1", "1.0.0-rc.1+build.1", true);
  test(SEMVER, "2.0.0-rc.1+build.123", "2.0.0-rc.1+build.123", true);
  test(SEMVER, "1.2.3-beta", "1.2.3-beta", true);
  test(SEMVER, "10.2.3-DEV-SNAPSHOT", "10.2.3-DEV-SNAPSHOT", true);
  test(SEMVER, "1.2.3-SNAPSHOT-123", "1.2.3-SNAPSHOT-123", true);
  test(SEMVER, "1.0.0", "1.0.0", true);
  test(SEMVER, "2.0.0", "2.0.0", true);
  test(SEMVER, "1.1.7", "1.1.7", true);
  test(SEMVER, "2.0.0+build.1848", "2.0.0+build.1848", true);
  test(SEMVER, "2.0.1-alpha.1227", "2.0.1-alpha.1227", true);
  test(SEMVER, "1.0.0-alpha+beta", "1.0.0-alpha+beta", true);
  test(SEMVER, "1.2.3----RC-SNAPSHOT.12.9.1--.12+788",
       "1.2.3----RC-SNAPSHOT.12.9.1--.12+788", true);
  test(SEMVER, "1.2.3----R-S.12.9.1--.12+meta", "1.2.3----R-S.12.9.1--.12+meta",
       true);
  test(SEMVER, "1.2.3----RC-SNAPSHOT.12.9.1--.12",
       "1.2.3----RC-SNAPSHOT.12.9.1--.12", true);
  test(SEMVER, "1.0.0+0.build.1-rc.10000aaa-kk-0.1",
       "1.0.0+0.build.1-rc.10000aaa-kk-0.1", true);
  test(SEMVER, "99999999999999999999999.999999999999999999.99999999999999999",
       "99999999999999999999999.999999999999999999.99999999999999999", true);
  test(SEMVER, "1.0.0-0A.is.legal", "1.0.0-0A.is.legal", true);

  test(SEMVER, "1", NULL, false);
  test(SEMVER, "1.2", NULL, false);
  test(SEMVER, "1.2.3-0123", "1.2.3-0", false);
  test(SEMVER, "1.2.3-0123.0123", "1.2.3-0", false);
  test(SEMVER, "1.1.2+.123", "1.1.2", false);
  test(SEMVER, "+invalid", NULL, false);
  test(SEMVER, "-invalid", NULL, false);
  test(SEMVER, "-invalid+invalid", NULL, false);
  test(SEMVER, "-invalid.01", NULL, false);
  test(SEMVER, "alpha", NULL, false);
  test(SEMVER, "alpha.beta", NULL, false);
  test(SEMVER, "alpha.beta.1", NULL, false);
  test(SEMVER, "alpha.1", NULL, false);
  test(SEMVER, "alpha+beta", NULL, false);
  test(SEMVER, "alpha_beta", NULL, false);
  test(SEMVER, "alpha.", NULL, false);
  test(SEMVER, "alpha..", NULL, false);
  test(SEMVER, "beta", NULL, false);
  test(SEMVER, "1.0.0-alpha_beta", "1.0.0-alpha", false);
  test(SEMVER, "-alpha.", NULL, false);
  test(SEMVER, "1.0.0-alpha..", "1.0.0-alpha", false);
  test(SEMVER, "1.0.0-alpha..1", "1.0.0-alpha", false);
  test(SEMVER, "1.0.0-alpha...1", "1.0.0-alpha", false);
  test(SEMVER, "1.0.0-alpha....1", "1.0.0-alpha", false);
  test(SEMVER, "1.0.0-alpha.....1", "1.0.0-alpha", false);
  test(SEMVER, "1.0.0-alpha......1", "1.0.0-alpha", false);
  test(SEMVER, "1.0.0-alpha.......1", "1.0.0-alpha", false);
  test(SEMVER, "01.1.1", "1.1.1", false);
  test(SEMVER, "1.01.1", NULL, false);
  test(SEMVER, "1.1.01", "1.1.0", false);
  test(SEMVER, "1.2", NULL, false);
  test(SEMVER, "1.2.3.DEV", "1.2.3", false);
  test(SEMVER, "1.2-SNAPSHOT", NULL, false);
  test(SEMVER, "1.2.31.2.3----RC-SNAPSHOT.12.09.1--..12+788", "1.2.31", false);
  test(SEMVER, "1.2-RC-SNAPSHOT", NULL, false);
  test(SEMVER, "-1.0.3-gamma+b7718", "1.0.3-gamma+b7718", false);
  test(SEMVER, "+justmeta", NULL, false);
  test(SEMVER, "9.8.7+meta+meta", "9.8.7+meta", false);
  test(SEMVER, "9.8.7-whatever+meta+meta", "9.8.7-whatever+meta", false);
  test(SEMVER,
       "99999999999999999999999.999999999999999999.99999999999999999"
       "----RC-SNAPSHOT.12.09.1--------------------------------..12",
       "99999999999999999999999.999999999999999999.99999999999999999"
       "----RC-SNAPSHOT.12.0",
       false);
}
