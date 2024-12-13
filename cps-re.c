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

static void match_atom(char *regex, char *input, struct ret *ret,
                       struct cont *cont);
static void do_star(char *regex, char *input, struct ret *ret,
                    struct cont *cont) {
  match_atom(regex, input, ret, &(struct cont){do_star, regex, cont});
  cont->fp(cont->regex, input, ret, cont->up);
  return; // backtrack
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

static char *skip_term(char *regex) {
skip_term:;
  char *quant = skip_atom(regex);
  if (quant == NULL)
    return regex;
  if (*quant && strchr("*+?", *quant))
    quant++;
  regex = quant;
  goto skip_term; // tail call
}

static void match_term(char *regex, char *input, struct ret *ret,
                       struct cont *cont) {
match_term:;
  char *quant = skip_atom(regex);
  if (quant == NULL) {
    cont->fp(cont->regex, input, ret, cont->up);
    return; // backtrack
  }

  switch (*quant) {
  case '*':
    do_star(regex, input, ret, &(struct cont){match_term, ++quant, cont});
    return; // backtrack
  case '+':
    match_atom(regex, input, ret,
               &(struct cont){do_star, regex,
                              &(struct cont){match_term, ++quant, cont}});
    return; // backtrack
  case '?':
    match_atom(regex, input, ret, &(struct cont){match_term, ++quant, cont});
    regex = quant;
    goto match_term; // tail call
  default:
    match_atom(regex, input, ret, &(struct cont){match_term, quant, cont});
    return; // backtrack
  }
}

static char *skip_regex(char *regex) {
skip_regex:;
  if (*(regex = skip_term(regex)) != '|')
    return regex;
  ++regex;
  goto skip_regex; // tail call
}

static void match_regex(char *regex, char *input, struct ret *ret,
                        struct cont *cont) {
match_regex:;
  char *alt = skip_term(regex);
  if (*alt != '|') {
    match_term(regex, input, ret, cont);
    return; // backtrack
  }

  match_term(regex, input, ret, cont);
  regex = ++alt;
  goto match_regex; // tail call
}

static void unwind(char *regex, char *input, struct ret *ret,
                   struct cont *cont) {
  ret->res = input, longjmp(ret->jmp_buf, 1);
}

char *cpsre_matches(char *regex, char *input) {
  if (*skip_regex(regex) != '\0')
    return CPSRE_SYNTAX;

  struct ret ret;
  if (setjmp(ret.jmp_buf) != 0)
    return ret.res;
  match_regex(regex, input, &ret, &(struct cont){unwind, NULL, NULL});
  return NULL;
}
