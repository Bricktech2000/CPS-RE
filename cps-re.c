#include "cps-re.h"
#include <setjmp.h>
#include <string.h>

#define METACHARS "\\.-*+?()|"

struct ret {
  char *loc;
  enum res res;
  jmp_buf jmp_buf;
};

struct cont {
  void (*fp)(char *regex, char *input, struct ret *ret, struct cont *cont);
  char *regex;
  struct cont *up;
};

// XXX doc using explicit returns
// XXX doc `goto` considered; means tail call

static void match_atom(char *regex, char *input, struct ret *ret,
                       struct cont *cont);
static void do_star(char *regex, char *input, struct ret *ret,
                    struct cont *cont) {
  match_atom(regex, input, ret, &(struct cont){do_star, regex, cont});
  cont->fp(cont->regex, input, ret, cont->up);
  return;
}

static char *skip_symbol(char *regex) {
  if (*regex == '\\' && regex[1] && strchr(METACHARS, regex[1]))
    return regex + 2;

  if (!strchr(METACHARS, *regex))
    return regex + 1;

  return NULL;
}

static char match_symbol(char *regex) {
  if (*regex == '\\' && regex[1] && strchr(METACHARS, regex[1]))
    return regex[1];

  if (!strchr(METACHARS, *regex))
    return *regex;

  return '\0';
}

static char *skip_atom(char *regex) {
  char *end = skip_symbol(regex);
  return end;
}

static void match_atom(char *regex, char *input, struct ret *ret,
                       struct cont *cont) {
  char sym = match_symbol(regex);
  if (sym == '\0')
    return;
  if (*input == sym)
    cont->fp(cont->regex, ++input, ret, cont->up);
  return;
}

static char *skip_term(char *regex) {
skip_term:;
  char *quant = skip_atom(regex);
  if (quant == NULL)
    return regex;
  if (*quant && strchr("*+?", *quant))
    quant++;
  regex = quant;
  goto skip_term;
}

static void match_term(char *regex, char *input, struct ret *ret,
                       struct cont *cont) {
match_term:;
  char *quant = skip_atom(regex);
  if (quant == NULL) {
    cont->fp(cont->regex, input, ret, cont->up);
    return;
  }

  switch (*quant) {
  case '*':
    do_star(regex, input, ret, &(struct cont){match_term, quant + 1, cont});
    return;
  case '+':
    match_atom(regex, input, ret,
               &(struct cont){do_star, regex,
                              &(struct cont){match_term, quant + 1, cont}});
    return;
  case '?':
    match_atom(regex, input, ret, &(struct cont){match_term, quant + 1, cont});
    regex = quant + 1;
    goto match_term;
  default:
    match_atom(regex, input, ret, &(struct cont){match_term, quant, cont});
    return;
  }
}

static char *skip_regex(char *regex) {
skip_regex:;
  char *alt = skip_term(regex);
  if (*alt != '|')
    return alt;
  regex = alt + 1;
  goto skip_regex;
}

static void match_regex(char *regex, char *input, struct ret *ret,
                        struct cont *cont) {
match_regex:;
  char *alt = skip_term(regex);
  if (*alt != '|') {
    match_term(regex, input, ret, cont);
    return;
  }

  match_term(regex, input, ret, cont);
  regex = alt + 1;
  goto match_regex;
}

static void unwind(char *regex, char *input, struct ret *ret,
                   struct cont *cont) {
  ret->loc = input, ret->res = MATCH, longjmp(ret->jmp_buf, 1);
}

// TODO return loc?
enum res matches(char *regex, char *input) {
  char *null = skip_regex(regex);
  if (*null != '\0')
    return SYNTAX;

  struct ret ret;
  if (setjmp(ret.jmp_buf) != 0)
    return ret.res;
  match_regex(regex, input, &ret, &(struct cont){unwind, NULL, NULL});
  return NOMATCH;
}
