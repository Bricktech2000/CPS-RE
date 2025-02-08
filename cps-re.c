#include "cps-re.h"
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>

// this regex engine walks regular expressions in continuation-passing style and
// uses the call stack as a backtrack stack. this means `return`s and `longjmp`s
// are actually backtracks. to make sense of the parser refer to `grammar.bnf`

#define METACHARS "\\.-^$*+?()|&~"

// a closure that holds knowledge of:
// - what matcher function to call next (`fp`)
// - where in the regex to continue matching (`regex`)
// - what to do when the match is successful (`up`)
// not all functions we store in `cont->fp` take a `regex` as their first
// argument, so often we repurpose `cont->regex` to store something else
#define CONT(...) (&(struct cont){__VA_ARGS__})
struct cont {
  void (*fp)(char *regex, char *input, struct cont *cont);
  char *regex;
  struct cont *up;
};

// small wrappers around `setjmp` and `longjmp` that let you "nest" and "unset"
// jump handlers. `LONGJMP(jmplist)` jumps up the stack to the closest `SETJMP
// (jmplist)` that has a matching `jmplist`. `UNSETJMP(jmplist)` temporarily
// disables the closest `SETJMP(jmplist)` that has a matching `jmplist`, crucial
// for continuation-passing style

struct jmplist {
  jmp_buf jmp_buf;
  struct jmplist *up;
};

#define SETJMP(JMPLIST)                                                        \
  for (struct jmplist *_jmp = &(struct jmplist){.up = JMPLIST}; _jmp;)         \
    for (JMPLIST = _jmp; _jmp; JMPLIST = _jmp->up, _jmp = NULL)                \
      if (setjmp(_jmp->jmp_buf) == 0)

#define UNSETJMP(JMPLIST)                                                      \
  for (struct jmplist *_jmp = JMPLIST; _jmp;)                                  \
    for (JMPLIST = JMPLIST->up; _jmp; JMPLIST = _jmp, _jmp = NULL)

#define CATCHJMP else
#define LONGJMP(JMPLIST) longjmp(JMPLIST->jmp_buf, 1)

static char *match_end;           // to store match end when a match is found
static struct jmplist *match_jmp; // to unwind the stack when a match is found
static struct jmplist *poss_jmp;  // to backtrack possessive quantifiers

static void found_match(char *target, char *input, struct cont *_cont) {
  // report a match by unwinding the stack to the closest `SETJMP(match_jmp)`.
  // if `target` is not null, only do so when the match found ends at `target`
  if (target == NULL || input == target)
    match_end = input, LONGJMP(match_jmp);
  return; // backtrack
}

static void require_progress(char *prev_input, char *input, struct cont *cont) {
  // backtrack if we've consumed no input since `prev_input`. used by `rep_...`
  // functions so regexes like /()*/ and /()+/ don't get stuck
  if (input != prev_input)
    cont->fp(cont->regex, input, cont->up);
  return; // backtrack
}

static void commit_possessive(char *_regex, char *input, struct cont *cont) {
  // run the continuation, but if it backtracks, jump to a "backtrack
  // checkpoint" for possessive quantifiers. this effectively "locks in"
  // a possessive quantifier
  UNSETJMP(poss_jmp) { cont->fp(cont->regex, input, cont->up); }
  LONGJMP(poss_jmp); // backtrack
}

static void match_atom(char *regex, char *input, struct cont *cont);

static void rep_greedy(char *regex, char *input, struct cont *cont) {
  match_atom(regex, input,
             CONT(require_progress, input, CONT(rep_greedy, regex, cont)));
  cont->fp(cont->regex, input, cont->up);
  return; // backtrack
}

static void rep_poss(char *regex, char *input, struct cont *cont) {
  match_atom(regex, input,
             CONT(require_progress, input, CONT(rep_poss, regex, cont)));
  commit_possessive(NULL, input, cont); // never returns
}

static void rep_lazy(char *regex, char *input, struct cont *cont) {
  cont->fp(cont->regex, input, cont->up);
  match_atom(regex, input,
             CONT(require_progress, input, CONT(rep_lazy, regex, cont)));
  return; // backtrack
}

static char *parse_symbol(char *regex, char *sym) {
  if (!strchr(METACHARS, *regex))
    return *sym = *regex, ++regex;
  if (*regex == '\\' && *++regex && strchr(METACHARS, *regex))
    return *sym = *regex, ++regex;
  return NULL; // syntax
}

static char *parse_regex(char *regex);
static char *parse_atom(char *regex) {
  if (*regex == '(') {
    if (*(regex = parse_regex(++regex)) == ')')
      return ++regex;
    return NULL; // syntax
  }
  *regex == '^' && regex++;
  if (*regex == '.')
    return ++regex;
  regex = parse_symbol(regex, &(char){0});
  if (regex != NULL && *regex == '-')
    return parse_symbol(++regex, &(char){0}); // syntax or ok
  return regex;                               // syntax or ok
}

static void match_regex(char *regex, char *input, struct cont *cont);
static void match_atom(char *regex, char *input, struct cont *cont) {
  if (*regex == '(') {
    if (*parse_regex(++regex) == ')')
      match_regex(regex, input, cont);
    return; // backtrack or syntax
  }

  bool complement = *regex == '^' && regex++;

  if (*regex == '.' && *input && true ^ complement) {
    cont->fp(cont->regex, ++input, cont->up);
    return; // backtrack
  }

  char lower, upper, temp;
  regex = parse_symbol(regex, &lower), upper = lower;
  if (regex != NULL && *regex == '-')
    regex = parse_symbol(++regex, &upper);
  if (regex == NULL)
    return;

  // character range wraparound
  if (lower > upper)
    temp = upper, upper = lower - 1, lower = temp + 1, complement = !complement;

  if (*input && (lower <= *input && *input <= upper) ^ complement)
    cont->fp(cont->regex, ++input, cont->up);
  return; // backtrack or syntax
}

static char *parse_factor(char *regex) {
  if ((regex = parse_atom(regex)) == NULL)
    return NULL; // syntax
  if (*regex && strchr("*+?", *regex))
    if (*++regex && strchr("+?", *regex))
      regex++;
  return regex;
}

static void match_factor(char *regex, char *input, struct cont *cont) {
  char *quant = parse_atom(regex);
  if (quant == NULL)
    return; // syntax

  bool poss = *quant && quant[1] == '+';
  bool lazy = *quant && quant[1] == '?';

  switch (*quant) {
  case '*':
    if (!poss)
      (lazy ? rep_lazy : rep_greedy)(regex, input, cont);
    else
      SETJMP(poss_jmp) { rep_poss(regex, input, cont); }
    return; // backtrack
  case '+':
    if (!poss)
      match_atom(regex, input, CONT(lazy ? rep_lazy : rep_greedy, regex, cont));
    else
      SETJMP(poss_jmp) {
        match_atom(regex, input, CONT(rep_poss, regex, cont));
      }
    return; // backtrack
  case '?':
    if (!poss)
      if (lazy)
        cont->fp(cont->regex, input, cont->up), match_atom(regex, input, cont);
      else
        match_atom(regex, input, cont), cont->fp(cont->regex, input, cont->up);
    else
      SETJMP(poss_jmp) {
        match_atom(regex, input, CONT(commit_possessive, NULL, cont));
        UNSETJMP(poss_jmp) { cont->fp(cont->regex, input, cont->up); }
      }
    return; // backtrack
  default:
    match_atom(regex, input, cont);
    return; // backtrack
  }
}

static char *parse_term(char *regex) {
  if (*regex == '~')
    regex++;
  for (char *term; (term = parse_factor(regex)) != NULL;)
    regex = term;
  return regex;
}

static void match_term(char *regex, char *input, struct cont *cont) {
  if (*regex == '~' && *++regex != '~') {
    // check whether the term being complemented matches the next 'n' characters
    // of input, starting with 'n := 0'. if it does not, call the continuation
    // to proceed; if it does, or if the continuation backtracks, try again with
    // 'n := n + 1' characters of input. unfortunately this overrules quantifier
    // greediness and laziness, meaning identities like `~(~a) == a` and `a&b ==
    // ~(~a|~b)` and `a|b == ~(~a&~b)` won't hold in general for partial matches
    char *target = input;
    do {
      SETJMP(match_jmp) {
        match_term(regex, input, CONT(found_match, target, NULL));
        UNSETJMP(match_jmp) { cont->fp(cont->regex, target, cont->up); }
      }
    } while (*target++);

    return; // backtrack
  }

  char *term = parse_factor(regex);
  if (term == NULL) {
    cont->fp(cont->regex, input, cont->up);
    return; // backtrack
  }

  match_factor(regex, input,
               *term == '~' ? cont : CONT(match_term, term, cont));
  return; // backtrack
}

static char *parse_regex(char *regex) {
  while (regex = parse_term(regex), *regex == '|' || *regex == '&')
    regex++;
  return regex;
}

static void int_rhs(char *regex, char *input, struct cont *cont) {
  // the left-hand side of the intersection matched (beginning at input position
  // `cont->regex` and ending at input position `input`), so check if we can get
  // the right-hand side to exact-match at those positions
  if (cpsre_anchored(regex, cont->regex, input) != NULL)
    cont = cont->up, cont->fp(cont->regex, input, cont->up);
  return; // backtrack
}

static void match_regex(char *regex, char *input, struct cont *cont) {
  char *binop = parse_term(regex);

  // alternation and intersection are right-associative
  if (*binop == '|')
    match_term(regex, input, cont), match_regex(++binop, input, cont);
  else if (*binop == '&')
    // if the left-hand side of the intersection matches, call `intr_rhs` with
    // a dummy continuation that holds the `input` position before the match
    match_term(regex, input, CONT(int_rhs, ++binop, CONT(NULL, input, cont)));
  else
    match_term(regex, input, cont);

  return; // backtrack
}

char *cpsre_parse(char *regex) { return parse_regex(regex); }

char *cpsre_anchored(char *regex, char *input, char *target) {
  SETJMP(match_jmp) {
    match_regex(regex, input, CONT(found_match, target, NULL));
    UNSETJMP(match_jmp) { return NULL; }
  }

  return match_end;
}

char *cpsre_unanchored(char *regex, char *input, char *target) {
  do {
    if (cpsre_anchored(regex, input, target) != NULL)
      return input;
  } while (*input++);

  return NULL;
}
