#ifndef PREPROC_H_INCLUDED

#define PREPROC_H_INCLUDED

/* WARNING: all preproc_x functions potentially realloc the        *
 *          provided string, so you want to use call them like so: *
 *          src = preproc_x(src);                                  */

/* set a custom function to use for determining    *
 * whether characters comprise a contiguous block; *
 * if (func == 0), the default method is used      */
void
preproc_usecontig(int usecontig(char *str));

/* back up preproc's contig function */
void *
preproc_getcontig(void);

/* get token length */
int
preproc_toklen(char *src);

/* strip quotes from a string */
void
preproc_strip_quotes(char **_str, int sz);


/* preproc_oo_struct: preprocess object-oriented structs        *
 * replaces every `struct type *name` with `void *name`, noting *
 * that every time it finds a `name.func(x)`, it expands it to  *
 * `type_func(name, x)`                                         */
char *
preproc_oo_struct(char *src);


/* replace all occurrences of token with whitespace */
char *
preproc_remove_token(char *src, char *tok);


/* overwrite all comments with whitespace; *
 * if (keep_cchars), control characters like '\n' are preserved */
char *
preproc_remove_comments(char *src, int keep_cchars);


/* overwrites all occurrences of tok with ntok */
char *
preproc_overwrite_token(char *src, char *tok, char *ntok);

/* preprocess #include directives */
void procIncludes(char** str_);

#endif /* PREPROC_H_INCLUDED */

