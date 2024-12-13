extern const char CPSRE_SYNTAX_SENTINEL;
#define CPSRE_SYNTAX (char *)&CPSRE_SYNTAX_SENTINEL

char *cpsre_matches(char *regex, char *input);
