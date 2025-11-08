// returns a pointer one past the end of a well-formed regular expression
// beginning at `regex`, which will always exist because the empty regular
// expression is well formed. to check whether an entire string is a well-
// formed regular expression, use the condition `*cpsre_parse(...) == '\0'`
char *cpsre_parse(char *regex);

// these routines have leftmost-first semantics and return `NULL` when no match
// is found. when a match is present, `cpsre_unanchored` returns the beginning
// of the match and `cpsre_anchored` returns the end. if `target` is non-null,
// matches will end one character before `target`, and otherwise matches can
// end at any position. `input` must be null-terminated even when a non-null
// `target` is supplied. assuming `end` is a pointer to the end of `input`,
//   - `cpsre_anchored(..., end)` matches /regex/
//   - `cpsre_anchored(..., NULL)` matches /regex%/
//   - `cpsre_unanchored(..., end)` matches /%regex/
//   - `cpsre_unanchored(..., NULL)` matches /%regex%/
// for a partial match, run `cpsre_unanchored(..., NULL)` to get the beginning
// of the match; if a match is found, run `cpsre_anchored(..., NULL)` to get
// the end of the match. for an exact match, run `cpsre_anchored(..., end)`,
// where `end` is a pointer to `input`'s null terminator
char *cpsre_anchored(char *regex, char *input, char *target);
char *cpsre_unanchored(char *regex, char *input, char *target);
