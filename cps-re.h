extern const char CPSRE_SYNTAX_SENTINEL;
#define CPSRE_SYNTAX (char *)&CPSRE_SYNTAX_SENTINEL

// returns `CPSRE_SYNTAX` on syntax error.
// returns match end when match found.
// returns `NULL` when no match found.
char *cpsre_matches(char *regex, char *input);
