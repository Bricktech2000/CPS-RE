#include "cps-re.h"
#include <setjmp.h>
#include <string.h>

// this regex engine walks regular expressions in continuation-passing style and
// uses the call stack as a backtrack stack. this means `return`s and `longjmp`s
// are actually backtracks. to make sense of the parser refer to `grammar.bnf`

const char CPSRE_SYNTAX_SENTINEL;
#define METACHARS "\\.-*+?()|"

// a closure that holds knowledge of:
// - what matcher function to call next (`fp`)
// - where in the regex to continue matching (`regex`)
// - what to do when the match is successful (`up`)
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

static char *match_input = NULL;  // to store match end when a match is found
static jmp_buf *match_jmp = NULL; // to unwind the stack when a match is found
static struct jmplist *poss_jmp = NULL; // to backtrack possessive quantifiers

static void require_progress(char *prev_input, char *input, struct cont *cont) {
  // backtrack if we've consumed no input since `prev_input`. used by `rep_...`
  // functions so regexes like /()*/ and /()+/ don't get stuck
  if (input != prev_input)
    cont->fp(cont->regex, input, cont->up);
  return; // backtrack
}

static void commit_possessive(char *regex, char *input, struct cont *cont) {
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
  commit_possessive(regex, input, cont); // never returns
}

static void rep_lazy(char *regex, char *input, struct cont *cont) {
  cont->fp(cont->regex, input, cont->up);
  match_atom(regex, input,
             CONT(require_progress, input, CONT(rep_lazy, regex, cont)));
  return; // backtrack
}

static char *skip_symbol(char *regex) {
  if (!strchr(METACHARS, *regex))
    return ++regex;
  if (*regex == '\\' && *++regex && strchr(METACHARS, *regex))
    return ++regex;
  return NULL; // syntax
}

static char match_symbol(char *regex) {
  if (!strchr(METACHARS, *regex))
    return *regex;

  if (*regex == '\\' && *++regex && strchr(METACHARS, *regex))
    return *regex;

  return '\0'; // syntax
}

static char *skip_regex(char *regex);
static char *skip_atom(char *regex) {
  if (*regex == '.')
    return ++regex;
  if (*regex == '(') {
    if (*(regex = skip_regex(++regex)) == ')')
      return ++regex;
    return NULL; // syntax
  }
  regex = skip_symbol(regex);
  if (regex != NULL && *regex == '-')
    return skip_symbol(++regex); // syntax or ok
  return regex;                  // syntax or ok
}

static void match_regex(char *regex, char *input, struct cont *cont);
static void match_atom(char *regex, char *input, struct cont *cont) {
  if (*regex == '.' && *input) {
    cont->fp(cont->regex, ++input, cont->up);
    return; // backtrack
  }

  if (*regex == '(') {
    if (*skip_regex(++regex) == ')')
      match_regex(regex, input, cont);
    return; // backtrack or syntax
  }

  char *sym = skip_symbol(regex);
  if (sym == NULL)
    return; // syntax
  char begin = match_symbol(regex), end = begin;
  if (*sym == '-')
    end = match_symbol(++sym);

  if (*input && begin <= *input && *input <= end)
    cont->fp(cont->regex, ++input, cont->up);
  return; // backtrack or syntax
}

static char *skip_factor(char *regex) {
  if ((regex = skip_atom(regex)) == NULL)
    return NULL; // syntax
  if (*regex && strchr("*+?", *regex))
    if (*++regex && strchr("+?", *regex))
      regex++;
  return regex;
}

static void match_factor(char *regex, char *input, struct cont *cont) {
  char *quant = skip_atom(regex);
  if (quant == NULL)
    return; // syntax

  int poss = *quant && quant[1] == '+';
  int lazy = *quant && quant[1] == '?';

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
        match_atom(regex, input, CONT(commit_possessive, regex, cont));
        cont->fp(cont->regex, input, cont->up);
      }
    return; // backtrack
  default:
    match_atom(regex, input, cont);
    return; // backtrack
  }
}

static char *skip_term(char *regex) {
  for (char *term; (term = skip_factor(regex)) != NULL;)
    regex = term;
  return regex;
}

static void match_term(char *regex, char *input, struct cont *cont) {
  char *term = skip_factor(regex);
  if (term == NULL) {
    cont->fp(cont->regex, input, cont->up);
    return; // backtrack
  }

  match_factor(regex, input, CONT(match_term, term, cont));
  return; // backtrack
}

static char *skip_regex(char *regex) {
  while (*(regex = skip_term(regex)) == '|')
    regex++;
  return regex;
}

static void match_regex(char *regex, char *input, struct cont *cont) {
  char *alt = skip_term(regex);
  if (*alt != '|') {
    match_term(regex, input, cont);
    return; // backtrack
  }

  match_term(regex, input, cont);
  match_regex(++alt, input, cont);
  return; // backtrack
}

static void found_match(char *regex, char *input, struct cont *cont) {
  match_input = input, longjmp(*match_jmp, 1);
}

char *cpsre_matches(char *regex, char *input) {
  if (*skip_regex(regex) != '\0')
    return CPSRE_SYNTAX; // return sentinel on syntax error

  match_jmp = &(jmp_buf){0};
  if (setjmp(*match_jmp) != 0)
    return match_input; // return match end when match found
  match_regex(regex, input, CONT(found_match, NULL, NULL));
  return NULL; // return `NULL` when no match found
}
