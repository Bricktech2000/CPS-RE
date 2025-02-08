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
  test("^.", "", NULL, false);
  test("^.*", "", "", true);
  test("^.+", "", NULL, false);
  test("^.?", "", "", true);
  test("()", "", "", true);
  test("()*", "", "", true);
  test("()+", "", "", true);
  test("()?", "", "", true);
  test("(())", "", "", true);
  test("()()", "", "", true);
  test("()", "a", "", false);
  test("a()", "a", "a", true);
  test("()a", "a", "a", true);
  test(" ", " ", " ", true);
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
  test("~(~a*)", "aa", "", true);
  test("~(~a+)", "aa", "a", true);
  test("~(~a?)", "a", "", true);
  test("~(~a*a)", "aa", "a", true);
  test("~(~a+a)", "aa", "aa", true);
  test("~(~a?a)", "a", "a", true);

  // parse errors (mostly from LTRE)
  test("abc)", NULL, NULL, false);
  test("(abc", NULL, NULL, false);
  test("+a", NULL, NULL, false);
  test("a|*", NULL, NULL, false);
  test("\\x0", NULL, NULL, false);
  test("\\zzz", NULL, NULL, false);
  test("\\b", NULL, NULL, false);
  test("\\t", NULL, NULL, false);
  test("^^a", NULL, NULL, false);
  test("a**", NULL, NULL, false);
  test("a+*", NULL, NULL, false);
  test("a?*", NULL, NULL, false);

  // nonstandard features (mostly from LTRE)
  test("^a", "z", "z", true);
  test("^a", "a", NULL, false);
  test("^\n", "\r", "\r", true);
  test("^\n", "\n", NULL, false);
  test("^.", "\n", NULL, false);
  test("^.", "a", NULL, false);
  test("^a-z*", "1A!2$B", "1A!2$B", true);
  test("^a-z*", "1aA", "1", false);
  test("a-z*", "abc", "abc", true);
  test("\\^", "^", "^", true);
  test("^\\^", "^", NULL, false);
  test(".", " ", " ", true);
  test("^.", " ", NULL, false);
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
  test("~0*", "", NULL, false);
  test("~0*", "0", NULL, false);
  test("~0*", "00", NULL, false);
  test("~0*", "001", "001", true);
  test("ab&cd", "", NULL, false);
  test("ab&cd", "ab", NULL, false);
  test("ab&cd", "cd", NULL, false);
  test(".*a.*&...", "ab", NULL, false);
  test(".*a.*&...", "abc", "abc", true);
  test(".*a.*&...", "bcd", NULL, false);
  test("a&b|c", "a", NULL, false);
  test("a&b|c", "b", NULL, false);
  test("a&b|c", "c", NULL, false);
  test("a|b&c", "a", "a", true);
  test("a|b&c", "b", NULL, false);
  test("a|b&c", "c", NULL, false);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "", NULL, false);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "abc", "abc", true);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "abc123", "abc123", true);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "1a2b3c", "1a2b3c", true);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "123", NULL, false);
  test("0x(~(0-9|a-f)+)", "0yz", NULL, false);
  test("0x(~(0-9|a-f)+)", "0x12", "0x", false);
  test("0x(~(0-9|a-f)+)", "0x", "0x", true);
  test("0x(~(0-9|a-f)+)", "0xy", "0x", true);
  test("0x(~(0-9|a-f)+)", "0xyz", "0x", true);
  test("b(~a*)", "", NULL, false);
  test("b(~a*)", "b", NULL, false);
  test("b(~a*)", "ba", NULL, false);
  test("b(~a*)", "bbaa", "bb", true);
  test("-", NULL, NULL, false);
  test("a-", NULL, NULL, false);
  test(".-a", NULL, NULL, false);
  test("a-.", NULL, NULL, false);
  test("~~a", NULL, NULL, false);
  test("a~b", NULL, NULL, false);

  // realistic regexes (mostly from LTRE)
#define HEX_RGB "#(...(...)?&(0-9|a-f|A-F)*?)"
  test(HEX_RGB, "000", NULL, false);
  test(HEX_RGB, "#0aA", "#0aA", true);
  test(HEX_RGB, "#00ff", "#00f", false);
  test(HEX_RGB, "#abcdef", "#abcdef", true);
  test(HEX_RGB, "#abcdeff", "#abcdef", false);
#define BLOCK_COMMENT "/\\*(~.*\\*/.*)\\*/"
#define LINE_COMMENT "//^\n*\n"
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
#define CONV_SPEC "%(" DI "|" U "|" OX "|" FEGA "|" C "|" S "|" P "|" N "|%)"
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
#define PRINTF_FMT "(^%|" CONV_SPEC ")*+"
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
#define HEX_QUAD "(....&(0-9|a-f|A-F)*?)"
#define IDENTIFIER                                                             \
  "(0-9|a-z|A-Z|_|\\\\u" HEX_QUAD "|\\\\U" HEX_QUAD HEX_QUAD ")++"             \
  "&~0-9.*&~" KEYWORD
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
#define HEX_QUAD "(....&(0-9|a-f|A-F)*?)"
#define JSON_STR                                                               \
  "\"( -!|#-[|]-\xff|\\\\(\"|\\\\|/|b|f|n|r|t)|\\\\u" HEX_QUAD ")*+\""
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
  "(\x01-\x7f|(\xc2-\xdf|\xe0\xa0-\xbf|\xed\x80-\x9f|(\xe1-\xec|\xee|\xef|"    \
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
}
