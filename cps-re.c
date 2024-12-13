#include "cps-re.h"
#include <setjmp.h>
#include <string.h>

const char CPSRE_SYNTAX_SENTINEL;
#define METACHARS "\\.-*+?()|"

struct ret {
  char *res;
  jmp_buf jmp_buf;
};

struct cont {
  void (*fp)(char *regex, char *input, struct ret *ret, struct cont *cont);
  char *regex;
  struct cont *up;
};

#define JMP_MATCH 1
#define JMP_POSS 2

#define POSS_BOUNDARY(STMT)                                                    \
  do {                                                                         \
    struct ret *ret_orig = ret;                                                \
    {                                                                          \
      struct ret *ret = &(struct ret){NULL};                                   \
      switch (setjmp(ret->jmp_buf)) {                                          \
      case 0:                                                                  \
        STMT return; /* backtrack */                                           \
      case JMP_MATCH:                                                          \
        ret_orig->res = ret->res;                                              \
        longjmp(ret_orig->jmp_buf, JMP_MATCH); /* thread through */            \
      case JMP_POSS:                                                           \
        return; /* backtrack */                                                \
      }                                                                        \
    }                                                                          \
  } while (0)

static void match_atom(char *regex, char *input, struct ret *ret,
                       struct cont *cont);

static void rep_greedy(char *regex, char *input, struct ret *ret,
                       struct cont *cont) {
  match_atom(regex, input, ret, &(struct cont){rep_greedy, regex, cont});
  cont->fp(cont->regex, input, ret, cont->up);
  return; // backtrack
}

static void opt_greedy(char *regex, char *input, struct ret *ret,
                       struct cont *cont) {
  match_atom(regex, input, ret, cont);
  cont->fp(cont->regex, input, ret, cont->up);
  return; // backtrack
}

static void rep_lazy(char *regex, char *input, struct ret *ret,
                     struct cont *cont) {
  cont->fp(cont->regex, input, ret, cont->up);
  match_atom(regex, input, ret, &(struct cont){rep_lazy, regex, cont});
  return; // backtrack
}

static void opt_lazy(char *regex, char *input, struct ret *ret,
                     struct cont *cont) {
  cont->fp(cont->regex, input, ret, cont->up);
  match_atom(regex, input, ret, cont);
  return; // backtrack
}

static void rep_poss(char *regex, char *input, struct ret *ret,
                     struct cont *cont) {
  match_atom(regex, input, ret, &(struct cont){rep_poss, regex, cont});
  cont->fp(cont->regex, input, ret, cont->up);
  longjmp(ret->jmp_buf, JMP_POSS); // backtrack
}

static void opt_poss(char *regex, char *input, struct ret *ret,
                     struct cont *cont) {
  cont->fp(cont->regex, input, ret, cont->up);
  longjmp(ret->jmp_buf, JMP_POSS); // backtrack
}

static char *skip_symbol(char *regex) {
  if (*regex == '\\' && regex[1] && strchr(METACHARS, regex[1]))
    return ++regex, ++regex;

  if (!strchr(METACHARS, *regex))
    return ++regex;

  return NULL; // syntax
}

static char match_symbol(char *regex) {
  if (*regex == '\\' && regex[1] && strchr(METACHARS, regex[1]))
    return regex[1];

  if (!strchr(METACHARS, *regex))
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
    return skip_symbol(++regex); // (maybe) syntax
  return regex;                  // (maybe) syntax
}

static void match_regex(char *regex, char *input, struct ret *ret,
                        struct cont *cont);
static void match_atom(char *regex, char *input, struct ret *ret,
                       struct cont *cont) {
  if (*regex == '.' && *input) {
    cont->fp(cont->regex, ++input, ret, cont->up);
    return; // backtrack
  }

  if (*regex == '(') {
    if (*skip_regex(++regex) == ')')
      match_regex(regex, input, ret, cont);
    return; // backtrack
  }

  char *sym = skip_symbol(regex);
  if (sym == NULL)
    return; // syntax
  char begin = match_symbol(regex), end = begin;
  if (*sym == '-')
    end = match_symbol(++sym);

  if (*input && begin <= *input && *input <= end)
    cont->fp(cont->regex, ++input, ret, cont->up);
  return; // backtrack
}

static char *skip_factor(char *regex) {
  if ((regex = skip_atom(regex)) == NULL)
    return NULL; // syntax
  if (*regex && strchr("*+?", *regex))
    if (*++regex && strchr("+?", *regex))
      regex++;
  return regex;
}

static void match_factor(char *regex, char *input, struct ret *ret,
                         struct cont *cont) {
  char *quant = skip_atom(regex);
  if (quant == NULL)
    return; // syntax

  int lazy = *quant && quant[1] == '?';
  int poss = *quant && quant[1] == '+';

  switch (*quant) {
  case '*':
    if (poss)
      POSS_BOUNDARY(rep_poss(regex, input, ret, cont););
    (lazy ? rep_lazy : rep_greedy)(regex, input, ret, cont);
    return; // backtrack
  case '+':
    if (poss)
      POSS_BOUNDARY(match_atom(regex, input, ret,
                               &(struct cont){rep_poss, regex, cont}););
    match_atom(regex, input, ret,
               &(struct cont){lazy ? rep_lazy : rep_greedy, regex, cont});
    return; // backtrack
  case '?':
    if (poss)
      POSS_BOUNDARY(
          match_atom(regex, input, ret, &(struct cont){opt_poss, regex, cont});
          cont->fp(cont->regex, input, ret, cont->up););
    (lazy ? opt_lazy : opt_greedy)(regex, input, ret, cont);
    return; // backtrack
  default:
    match_atom(regex, input, ret, cont);
    return; // backtrack
  }
}

static char *skip_term(char *regex) {
  for (char *term; (term = skip_factor(regex)) != NULL;)
    regex = term;
  return regex;
}

static void match_term(char *regex, char *input, struct ret *ret,
                       struct cont *cont) {
  if (skip_factor(regex) == NULL)
    cont->fp(cont->regex, input, ret, cont->up);
  match_factor(regex, input, ret,
               &(struct cont){match_term, skip_factor(regex), cont});
  return; // backtrack
}

static char *skip_regex(char *regex) {
  while (*(regex = skip_term(regex)) == '|')
    regex++;
  return regex;
}

static void match_regex(char *regex, char *input, struct ret *ret,
                        struct cont *cont) {
  char *alt = skip_term(regex);
  if (*alt != '|') {
    match_term(regex, input, ret, cont);
    return; // backtrack
  }

  match_term(regex, input, ret, cont);
  match_regex(++alt, input, ret, cont);
  return; // backtrack
}

static void jmp_match(char *regex, char *input, struct ret *ret,
                      struct cont *cont) {
  ret->res = input, longjmp(ret->jmp_buf, JMP_MATCH);
}

char *cpsre_matches(char *regex, char *input) {
  if (*skip_regex(regex) != '\0')
    return CPSRE_SYNTAX;

  struct ret ret;
  if (setjmp(ret.jmp_buf) != 0)
    return ret.res;
  match_regex(regex, input, &ret, &(struct cont){jmp_match, NULL, NULL});
  return NULL;
}
