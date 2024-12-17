extern const char CPSRE_SYNTAX_SENTINEL;
#define CPSRE_SYNTAX (char *)&CPSRE_SYNTAX_SENTINEL

// these routines return `CPSRE_SYNTAX` on syntax error and `NULL` when no
// match is found. they have leftmost-first semantics. for a partial match,
// run `cpsre_matchbegin` to get the beginning of the match; if no errors
// occur, run `cpsre_matchend` to get the end of the match. for an exact
// match, run `cpsre_matchend` to get the end of the match; if no errors
// occur, ensure that `*end == '\0'`
char *cpsre_matchbegin(char *regex, char *input); // matches /.*regex.*/
char *cpsre_matchend(char *regex, char *input);   // matches /regex.*/
