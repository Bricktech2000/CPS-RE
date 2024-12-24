# CPS-RE

_A tiny regex engine in continuation-passing style_

CPS-RE is a tiny 200-line backtracking regex engine written in C99—but with a twist: it walks regular expressions in continuation-passing style, uses the call stack as its backtrack stack, and reports matches with a `longjmp` all the way back.

The engine supports, roughly in increasing order of precedence, alternation and intersection with infix `|` and infix `&`, complementation with prefix `~`, concatenation with juxtaposition, repetition with postfix `*` `+` `?` (including possessive `*+` `++` `?+` and lazy `*?` `+?` `??` variants), character complements with prefix `^`, character ranges with infix `-`, metacharacter escapes with prefix `\`, grouping with circumfix `()`, and wildcards with `.`. For more information see [grammar.bnf](grammar.bnf).

Alternation and intersection are right-associative. Prefixing a character or character range with `^` complements it. Character ranges support wraparound. Character classes are not supported. `.` matches any character, including newlines. The empty regular expression matches the empty word; to match no word, use `^.`.

Run the test suite with:

```bash
make test && bin/test
```
