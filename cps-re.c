#include "cps-re.h"
#include <setjmp.h>
#include <string.h>

// this regex engine matches regular expressions in continuation-passing
// style and uses the hardware call stack as a backtrack stack. this means
// `return`s and `longjmp`s are actually backtracks. to make sense of the
// parser refer to `grammar.bnf`

const char CPSRE_SYNTAX_SENTINEL;
#define METACHARS "\\.-*+?()|"

// an "exception thrower" struct that holds knowledge of:
// - what "exception handler" to jump to next (`jmp_buf`)
// - where the end of a match is, if `JMP_MATCH` was used (`input`)
// - how to throw to the parent "exception handler" (`up`)
struct jmp {
  jmp_buf jmp_buf;
  char *input;
  struct jmp *up;
};

// a closure that holds knowledge of:
// - what matcher function to call next (`fp`)
// - where in the regex to continue matching (`regex`)
// - what to do when the match is successful (`up`)
#define CONT(...) (&(struct cont){__VA_ARGS__})
struct cont {
  void (*fp)(char *regex, char *input, struct jmp *jmp, struct cont *cont);
  char *regex;
  struct cont *up;
};

#define JMP_MATCH 1 // `longjmp` to report a match was found
#define JMP_POSS 2  // `longjmp` to backtrack a possessive quantifier

// creates a backtrack boundary for possessive quantifiers. catches calls to
// `longjmp(..., JMP_POSS)` but is transparent to `longjmp(..., JMP_MATCH)`
#define CATCH_POSS(STMT)                                                       \
  do {                                                                         \
    struct jmp *up = jmp;                                                      \
    {                                                                          \
      struct jmp *jmp = &(struct jmp){.up = up};                               \
      switch (setjmp(jmp->jmp_buf)) {                                          \
      case 0:                                                                  \
        STMT break;                                                            \
      case JMP_MATCH:                                                          \
        up->input = jmp->input;                                                \
        longjmp(up->jmp_buf, JMP_MATCH); /* thread through */                  \
      case JMP_POSS:                                                           \
        return; /* backtrack */                                                \
      }                                                                        \
    }                                                                          \
  } while (0)

static void require_progress(char *prev_input, char *input, struct jmp *jmp,
                             struct cont *cont) {
  // backtrack if we've consumed no input since `prev_input`. used in /*/ and
  // /+/ so regexes like /()*/ and /()+/ don't get stuck
  if (input != prev_input)
    cont->fp(cont->regex, input, jmp, cont->up);
  return; // backtrack
}

static void match_atom(char *regex, char *input, struct jmp *jmp,
                       struct cont *cont);

static void rep_greedy(char *regex, char *input, struct jmp *jmp,
                       struct cont *cont) {
  match_atom(regex, input, jmp,
             CONT(require_progress, input, CONT(rep_greedy, regex, cont)));
  cont->fp(cont->regex, input, jmp, cont->up);
  return; // backtrack
}

static void rep_poss(char *regex, char *input, struct jmp *jmp,
                     struct cont *cont) {
  match_atom(regex, input, jmp,
             CONT(require_progress, input, CONT(rep_poss, regex, cont)));
  cont->fp(cont->regex, input, jmp->up, cont->up);
  longjmp(jmp->jmp_buf, JMP_POSS); // backtrack
}

static void rep_lazy(char *regex, char *input, struct jmp *jmp,
                     struct cont *cont) {
  cont->fp(cont->regex, input, jmp, cont->up);
  match_atom(regex, input, jmp,
             CONT(require_progress, input, CONT(rep_lazy, regex, cont)));
  return; // backtrack
}

static void opt_greedy(char *regex, char *input, struct jmp *jmp,
                       struct cont *cont) {
  match_atom(regex, input, jmp, cont);
  cont->fp(cont->regex, input, jmp, cont->up);
  return; // backtrack
}

static void opt_poss(char *regex, char *input, struct jmp *jmp,
                     struct cont *cont) {
  cont->fp(cont->regex, input, jmp->up, cont->up);
  longjmp(jmp->jmp_buf, JMP_POSS); // backtrack
}

static void opt_lazy(char *regex, char *input, struct jmp *jmp,
                     struct cont *cont) {
  cont->fp(cont->regex, input, jmp, cont->up);
  match_atom(regex, input, jmp, cont);
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

static void match_regex(char *regex, char *input, struct jmp *jmp,
                        struct cont *cont);
static void match_atom(char *regex, char *input, struct jmp *jmp,
                       struct cont *cont) {
  if (*regex == '.' && *input) {
    cont->fp(cont->regex, ++input, jmp, cont->up);
    return; // backtrack
  }

  if (*regex == '(') {
    if (*skip_regex(++regex) == ')')
      match_regex(regex, input, jmp, cont);
    return; // backtrack or syntax
  }

  char *sym = skip_symbol(regex);
  if (sym == NULL)
    return; // syntax
  char begin = match_symbol(regex), end = begin;
  if (*sym == '-')
    end = match_symbol(++sym);

  if (*input && begin <= *input && *input <= end)
    cont->fp(cont->regex, ++input, jmp, cont->up);
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

static void match_factor(char *regex, char *input, struct jmp *jmp,
                         struct cont *cont) {
  char *quant = skip_atom(regex);
  if (quant == NULL)
    return; // syntax

  int poss = *quant && quant[1] == '+';
  int lazy = *quant && quant[1] == '?';

  switch (*quant) {
  case '*':
    if (poss)
      CATCH_POSS(rep_poss(regex, input, jmp, cont););
    else
      (lazy ? rep_lazy : rep_greedy)(regex, input, jmp, cont);
    return; // backtrack
  case '+':
    if (poss)
      CATCH_POSS(match_atom(regex, input, jmp, CONT(rep_poss, regex, cont)););
    else
      match_atom(regex, input, jmp,
                 CONT(lazy ? rep_lazy : rep_greedy, regex, cont));
    return; // backtrack
  case '?':
    if (poss)
      CATCH_POSS(match_atom(regex, input, jmp, CONT(opt_poss, regex, cont));
                 cont->fp(cont->regex, input, jmp, cont->up););
    else
      (lazy ? opt_lazy : opt_greedy)(regex, input, jmp, cont);
    return; // backtrack
  default:
    match_atom(regex, input, jmp, cont);
    return; // backtrack
  }
}

static char *skip_term(char *regex) {
  for (char *term; (term = skip_factor(regex)) != NULL;)
    regex = term;
  return regex;
}

static void match_term(char *regex, char *input, struct jmp *jmp,
                       struct cont *cont) {
  char *term = skip_factor(regex);
  if (term == NULL) {
    cont->fp(cont->regex, input, jmp, cont->up);
    return; // backtrack
  }

  match_factor(regex, input, jmp, CONT(match_term, term, cont));
  return; // backtrack
}

static char *skip_regex(char *regex) {
  while (*(regex = skip_term(regex)) == '|')
    regex++;
  return regex;
}

static void match_regex(char *regex, char *input, struct jmp *jmp,
                        struct cont *cont) {
  char *alt = skip_term(regex);
  if (*alt != '|') {
    match_term(regex, input, jmp, cont);
    return; // backtrack
  }

  match_term(regex, input, jmp, cont);
  match_regex(++alt, input, jmp, cont);
  return; // backtrack
}

static void jmp_match(char *regex, char *input, struct jmp *jmp,
                      struct cont *cont) {
  // a match was found. unwind the stack
  jmp->input = input, longjmp(jmp->jmp_buf, JMP_MATCH);
}

char *cpsre_matches(char *regex, char *input) {
  if (*skip_regex(regex) != '\0')
    return CPSRE_SYNTAX; // return sentinel on syntax error

  struct jmp jmp = {.up = NULL};
  if (setjmp(jmp.jmp_buf) != 0)
    return jmp.input; // return match end when match found
  match_regex(regex, input, &jmp, CONT(jmp_match, NULL, NULL));
  return NULL; // return `NULL` when no match found
}
