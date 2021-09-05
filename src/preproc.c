/* preproc.c contains some preprocessing functions */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

static int (*iscontig)(char *str) = 0;

/* types */
struct typename
{
	char *type;
	char *name;
	struct typename *next;
};

struct scope
{
	struct scope *parent;
	struct typename *type;
};

/****
 * internal use only functions
 ****/

/* free typename */
static
void
typename_free(struct typename *type)
{
	if (!type)
		return;
	
	/* free all in list */
	while (type)
	{
		struct typename *next = type->next;
		
		if (type->type)
			free(type->type);
		
		if (type->name)
			free(type->name);
		
		free(type);
		
		type = next;
	}
}

/* free scope */
static
void
scope_free(struct scope *scope)
{
	if (!scope)
		return;
	
	if (scope->type)
		typename_free(scope->type);
	
	free(scope);
}

/* search scope for a typename */
struct typename *
scope_search_name(struct scope *scope, char *str, int len)
{
	if (!scope || !str || !len)
		return 0;
	
	while (scope)
	{
		struct typename *typename;
		
		typename = scope->type;
		
		while (typename)
		{
			/* if name fields match */
			if (strlen(typename->name) == len && !memcmp(typename->name, str, len))
				return typename;
			
			/* go to next in list */
			typename = typename->next;
		}
		
		/* broaden scope */
		scope = scope->parent;
	}
	
	return 0;
}

/* safe calloc */
static
void *
calloc_safe(size_t nmemb, size_t size)
{
	void *result;
	
	result = calloc(nmemb, size);
	
	if (!result)
	{
		fprintf(stderr, "[!] allocation failure\n");
		exit(EXIT_FAILURE);
	}
	
	return result;
}

/* safe malloc */
static
void *
malloc_safe(size_t size)
{
	void *result;
	
	result = malloc(size);
	
	if (!result)
	{
		fprintf(stderr, "[!] allocation failure\n");
		exit(EXIT_FAILURE);
	}
	
	return result;
}

/* safe realloc */
static
void *
realloc_safe(void *ptr, size_t size)
{
	void *result;
	
	result = realloc(ptr, size);
	
	if (!result)
	{
		fprintf(stderr, "[!] allocation failure\n");
		exit(EXIT_FAILURE);
	}
	
	return result;
}

/* insert characters inside a string */
static
void
string_insert_num(char *str, int n)
{
	size_t len = strlen(str) + 1;
	
	memmove(str + n, str, len);
}

/* realloc a string to have extra space at the end */
static
char *
string_lengthen(char *src, size_t pad)
{
	size_t st;
	
	assert(src);
	
	if (!pad)
		return src;
	
	st = strlen(src);
	src = realloc_safe(src, st + pad);
	memset(src + st, 0, pad);
	
	return src;
}

/* safe strndup */
static
char *
strndup_safe(char *str, size_t n)
{
	char *result;
	
	result = malloc_safe(n + 1);
	
	memcpy(result, str, n);
	
	result[n] = '\0';
	
	return result;
}

/* character is whitespace */
static
int
iswhite(int c)
{
	return (c <= ' ' && c > '\0');
}

/* string is comment */
static
int
iscomment(char *s)
{
	return (*s && s[1] && *s == '/' && (s[1] == '*' || s[1] == '/'));
}

/* get length of token */
static
int
get_tok_len(char *src)
{
	int len = 0;
	
	assert(src);
	
	/* end of string */
	if (!*src)
		return 0;
	
	/* white space should be treated as a contiguous block */
	if (iswhite(*src))
	{
		while (iswhite(*src))
		{
			len += 1;
			src += 1;
		}
		return len;
	}
	
	/* comments should be treated as contiguous blocks */
	if (*src == '/' && src[1] == '*')
	{
		while (*src)
		{
			src += 1;
			len += 1;
			if (*src == '*' && src[1] == '/')
				break;
		}
		
		/* end of string */
		if (!*src)
			return len;
		
		return len + 2;
	}
	
	/* single-line comments are treated as contiguous blocks */
	if (*src == '/' && src[1] == '/')
	{
		while (*src && *src != '\n')
		{
			src += 1;
			len += 1;
		}
		
		return len;
	}
	
	/* strings should be treated as contiguous blocks */
	if (*src == '"')
	{
		++src;
		++len;
		
		/* string is literally "" */
		if (*src == '"')
			return len + 1;
		
		/* seek end quote */
		while (*src && *src != '"')
		{
			if (*src == '\\')
			{
				src += 1;
				len += 1;
			}
			src += 1;
			len += 1;
		}
		
		return len + 1;
	}
	
	/* character specifiers should be treated as contiguous blocks */
	if (*src == '\'')
	{
		++src;
		++len;
		
		/* string is literally '' */
		if (*src == '\'')
			return len + 1;
		
		/* seek end quote */
		while (*src && *src != '\'')
		{
			if (*src == '\\')
			{
				src += 1;
				len += 1;
			}
			src += 1;
			len += 1;
		}
		
		return len + 1;
	}
	
	/* user-defined function */
	if (iscontig && (len = iscontig(src)))
	{
		return len;
	}
	
	/* text and numbers are treated as contiguous blocks */
	while (isalnum(*src) || *src == '_')
	{
		len += 1;
		src += 1;
	}
	
	if (!len)
		len = 1;
	
	return len;
}

/* preproc_oo_struct: preprocess object-oriented structs */
/* WARNING: you want to call src = preproc_oo_struct(src) b/c this
            reallocs the provided string */
/* replaces every `struct type *name` with `void *name`, noting
   that every time it finds a `name.func(x)`, it expands it to
   `type_func(name, x)` */
char *
preproc_oo_struct(char *src)
{
	struct typename *type_work = 0;
	struct scope *scope;
	struct scope *fun_scope = 0;
	struct scope *global_scope;
	char *insert_after_paren = 0;
	char *str;
	struct {
		char *str;
		int len;
	} last_tok = {0};
	int max_len = 0;
	int active = 0;
	int num_period = 0;
	int len = 0;
	
	/* step through every token and find longest struct-related token */
	for (str = src; *str; str += len)
	{
		/* get length of token at current position in string */
		len = get_tok_len(str);
		
		/* skip whitespace */
		if (iswhite(*str))
			continue;
		
		else if (*str == '.')
			num_period += 1;
		
		/* test the length of the two tokens that follow `struct` */
		if (active)
		{
			if (len > max_len)
				max_len = len;
			
			active += 1;
			if (active == 3)
				active = 0;
			
			continue;
		}
		
		/* add struct type to scope */
		else if (!memcmp("struct", str, len) && (isblank(str[len]) || iscntrl(str[len])))
			active = 1;
	}
	
	/* account for added ", " */
	max_len += 3;
	
	/* at this point, we resize the string to account for the code
	   we'll be adding */
	src = string_lengthen(src, max_len * 2 * num_period);
	
	/* allocate global scope */
	global_scope = calloc_safe(1, sizeof(*global_scope));
	scope = global_scope;
	
	/* now step through every token */
	for (str = src; *str; str += len)
	{
		/* get length of token at current position in string */
		len = get_tok_len(str);
		
		/* skip whitespace */
		if (iswhite(*str))
			continue;
		
		/* propagate contents of working type */
		if (type_work)
		{
			/* skip non-alpha-numeric characters */
			if (!isalnum(*str))
				continue;
			
			/* type name not yet initialized */
			if (!type_work->type)
			{
				type_work->type = strndup_safe(str, len);
				
				/* replace symbol within source with whitespace */
				/* at this point, we are at void          name; */
				memset(str, ' ', len);
			}
			
			/* alias not yet initialized */
			else if (!type_work->name)
			{
				type_work->name = strndup_safe(str, len);
				type_work = 0;
			}
		}
		
		/* push scope */
		if (*str == '{')
		{
			struct scope *newscope = calloc_safe(1, sizeof(*newscope));
			
			newscope->parent = scope;
			
			scope = newscope;
		}
		
		/* pop scope */
		else if (*str == '}')
		{
			if (!scope || scope == global_scope)
			{
				fprintf(stderr, "preprocessor error: unexpected '}'\n");
				exit(EXIT_FAILURE);
			}
			
			struct scope *parent;
			
popscope:
			parent = scope->parent;			
			scope_free(scope);			
			scope = parent;
			
			/* if current scope is function scope, pop it as well to
			   return to global scope */
			if (scope == fun_scope)
				goto popscope;
		}
		
		/* test '.' magic */
		else if (*str == '.')
		{
			/* if '.' is preceeded by an alpha-numeric token */
			if (last_tok.str && isalnum(*last_tok.str))
			{
				struct typename *type;
				
				/* search scope to see if this name belongs to
				   a structure */
				type = scope_search_name(scope, last_tok.str, last_tok.len);
				
				/* it does */
				if (type)
				{
					/* replace name.func with type_func */
					*str = '_';
					
					int nlen = strlen(type->name);
					int tlen = strlen(type->type);
					char *dst = last_tok.str;
					
					/* simple overwrite */
					if (nlen == tlen)
						memcpy(dst, type->type, tlen);
					
					/* overwriting with something smaller */
					else if (tlen < nlen)
					{
						/* fill first bit with spaces */
						memset(dst, ' ', nlen - tlen);
						
						/* overwrite the rest */
						memcpy(dst + (nlen - tlen), type->type, tlen);
					}
					
					/* must insert bytes before overwrite */
					else
					{
						string_insert_num(dst, tlen - nlen);
						memcpy(dst, type->type, tlen);
					}
					
					/* after next '(', insert the name of the token */
					insert_after_paren = type->name;
				}
			}
		}
		
		/* test '(' */
		else if (*str == '(')
		{
			/* if we are in global scope, this is a function */
			if (scope == global_scope)
			{
				/* this is the same thing that happens in '{' */
				struct scope *newscope = calloc_safe(1, sizeof(*newscope));
				
				newscope->parent = scope;				
				scope = newscope;
				
				/* function scope */
				fun_scope = scope;
			}
			
			/* if there is something to insert after parenthesis */
			else if (insert_after_paren)
			{
				char *dst = str + 1;
				int plen = strlen(insert_after_paren);
				
				/* insert string here followed by comma */
				string_insert_num(dst, plen + 2);
				
				memcpy(dst, insert_after_paren, plen);
				dst += plen;
				dst[0] = ',';
				dst[1] = ' ';
				insert_after_paren = 0;
			}
		}
		
		/* test ')' */
		else if (*str == ')')
		{
			/* if the token preceding this one is a comma, white it out */
			if (last_tok.str && last_tok.len && *last_tok.str == ',')
				memset(last_tok.str, ' ', last_tok.len);
		}
		
		/* add struct type to scope */
		else if (!memcmp("struct", str, len) && (isblank(str[len]) || iscntrl(str[len])))
		{
			struct typename *type;
			
			type = calloc_safe(1, sizeof(*type));
			
			/* set allocated type as type being worked on;
			   as new tokens are encountered, its contents
			   will be propagated */
			type_work = type;
			
			/* replace "struct" with "void  " */
			/* at this point, we are at void    type  name; */
			memcpy(str, "void  ", 6);
			
			/* link into linked list */
			if (!scope->type)
			{
				scope->type = type;
				type->next = 0;
			}
			else
			{
				type->next = scope->type->next;
				scope->type->next = type;
			}
		}
		
		/* note last values */
		last_tok.str = str;
		last_tok.len = len;
		
#if 0
		/* display the token */
		fprintf(stderr, "%.*s\n", len, str);
#endif
	}
	
	/* cleanup */
	scope_free(scope);
	
	return src;
}

/* strip quotes from a string */
void
preproc_strip_quotes(char **_str, int sz)
{
	char *str;
	
	assert(_str);
	assert(*_str);
	assert(sz);
	
	str = *_str;
	
	/* if it is a quote, strip quotes */
	if (*str == '"' || *str == '\'')
	{
		*_str += 1;
		str[sz-1] = '\0';
	}
}

#if 0
/* preproc_do_while: preprocess object-oriented structs */
/* WARNING: you want to call src = preproc_do_while(src) b/c this
            reallocs the provided string */
/* replaces do { ... } while(x); with
	int __ECOND_z;  // NOTE: z is the nth `do` encounter, and
	__ECOND_z = 1;  //       is declared at start of function
	while(__ECOND_z)
	{
		...
		if (!(x))
			__ECOND_z = 0;
	}
*/
char *
preproc_do_while(char *src)
{
	return src;
}
#endif

/* overwrites all occurrences of token with whitespace */
char *
preproc_remove_token(char *src, char *tok)
{
	char *str;
	int tok_len;
	int len;
	
	assert(tok);
	assert(src);
	
	tok_len = strlen(tok);
	
	for (str = src; *str; str += len)
	{
		/* get length of token at current position in string */
		len = get_tok_len(str);
		
		/* skip whitespace */
		if (iswhite(*str))
			continue;
		
		/* skip mismatched length */
		if (len != tok_len)
			continue;
		
		/* if it matches, overwrite with whitespace */
		if (!memcmp(str, tok, len))
			memset(str, ' ', len);
#if 0
		fprintf(stderr, "%.*s\n", len, str);
#endif
	}
	return src;
}

/* overwrite all comments with whitespace; *
 * if (keep_cchars), control characters like '\n' are preserved */
char *
preproc_remove_comments(char *src, int keep_cchars)
{
	char *str;
	int len;
	
	assert(src);
	
	for (str = src; *str; str += len)
	{
		/* get length of token at current position in string */
		len = get_tok_len(str);
		
		/* test if it grabbed a comment */
		if (iscomment(str))
		{
			/* a loop is used to preserve control characters */
			if (keep_cchars)
			{
				char *ss = str;
				int i;
				for (i = 0; i < len; ++i, ++ss)
					if (*ss > ' ')
						*ss = ' ';
			}
			
			/* overwrite the entire comment with whitespace otherwise */
			else
				memset(str, ' ', len);
		}
	}
	return src;
}

/* overwrites all occurrences of tok with ntok */
char *
preproc_overwrite_token(char *src, char *tok, char *ntok)
{
	char *str;
	int len;
	int tlen;
	int nlen;
	
	assert(src);
	assert(tok);
	assert(ntok);
	
	tlen = strlen(tok);
	nlen = strlen(ntok);
	
	/* new token is longer than old token; realloc is necessary */
	if (nlen > tlen)
	{
		int num = 0;
		
		/* count occurrences of token */
		for (str = src; *str; str += len)
		{
			/* get length of token at current position in string */
			len = get_tok_len(str);
			
			/* token matches */
			if (len == tlen && !memcmp(str, tok, len))
				num += 1;
		}
		
		/* add (nlen - tlen) bytes for each one found */
		src = string_lengthen(src, num * (nlen - tlen));
	}
	
	/* now overwrite every tok with ntok */
	for (str = src; *str; str += len)
	{
		/* get length of token at current position in string */
		len = get_tok_len(str);
		
		/* token mismatch */
		if (len != tlen || memcmp(str, tok, len))
			continue;
		
		/* simple overwrite */
		if (nlen == tlen)
			memcpy(str, ntok, tlen);
		
		/* overwriting with something smaller */
		else if (nlen < tlen)
		{
			/* fill first bit with spaces */
			memset(str, ' ', tlen - nlen);
			
			/* overwrite the rest */
			memcpy(str + (tlen - nlen), ntok, nlen);
		}
		
		/* must insert bytes before overwrite */
		else
		{
			string_insert_num(str, nlen - tlen);
			memcpy(str, ntok, nlen);
		}
	}
	return src;
}

/* get token length */
int
preproc_toklen(char *src)
{
	assert(src);
	
	return get_tok_len(src);
}

/* back up preproc's contig function */
void *
preproc_getcontig(void)
{
	return iscontig;
}

/* set a custom function to use for determining    *
 * whether characters comprise a contiguous block; *
 * if (func == 0), the default method is used      */
void
preproc_usecontig(int usecontig(char *str))
{
	iscontig = usecontig;
}

/* loads file as string */
void* loadstring(const char* fn) {
	FILE* fp;
	char* dat;
	size_t sz;

	/* rudimentary error checking returns 0 on any error */
	if (
		!fn
		|| !(fp = fopen(fn, "rb"))
		|| fseek(fp, 0, SEEK_END)
		|| !(sz = ftell(fp))
		|| fseek(fp, 0, SEEK_SET)
		|| !(dat = malloc(sz + 1))
		|| fread(dat, 1, sz, fp) != sz
		|| fclose(fp)
		)
		return 0;

	dat[sz] = '\0';
	return dat;
}

/* loads file as string (with error checking) */
void* loadstringSafe(const char* fn) {
	void* dat = loadstring(fn);

	if (!dat) 	{
		fprintf(stderr, "failed to load file '%s'\n", fn);
		exit(EXIT_FAILURE);
	}

	return dat;
}

void stringInsert(char** str_, char* start, char* end, const char* src) {
	char* str = *str_;

	memset(start, ' ', (end - start) + 1);

	/* can fit without realloc */
	if ((end - start) >= strlen(src)) 	{
		memcpy(start, src, strlen(src));
		return;
	}

	/* resize string to fit new data */
	/* TODO optimization: add only the exact number of bytes necessary */
	str = realloc(str, strlen(str) + 1 + strlen(src) + 1);
	start = (start - *str_) + str;
	end = (end - *str_) + str;
	memmove(start + strlen(src), end, strlen(end) + 1);
	memcpy(start, src, strlen(src));

	*str_ = str;
}

void procIncludes(char** str_) {
	char* str = *str_;

	while (1) 	{
		char* start;
		char* this;
		char* end;
		char* include;

		if (!(this = strstr(str, "#include")))
			break;

		/* assume only `#include "wow.h"` style includes */
		start = this + strcspn(this, "\"") + 1;
		end = strchr(start, '"');

		/* skip unsupported include */
		if (!end) 		{
			/* TODO error message */
			printf("bad include\n");
			*this = ' ';
			continue;
		}

		/* load and process include */
		*end = '\0';
		fprintf(stderr, "load '%s'\n", start);
		include = loadstringSafe(start);
		procIncludes(&include);
		stringInsert(&str, this, end, include);
		free(include);
	}

	*str_ = str;
}

#ifdef PREPROC_TEST
int
main(int argc, char *argv[])
{
	FILE    *fp;
	char    *raw;
	size_t   raw_sz;
	
	if (argc < 2)
	{
		fprintf(stderr, "args: program path/to/source.c\n");
		return 1;
	}
	
	fp = fopen(argv[1], "rb");
	if (!fp)
		return 1;
	
	fseek(fp, 0, SEEK_END);
	raw_sz = ftell(fp);
	if (!raw_sz)
		goto fail;
	
	fseek(fp, 0, SEEK_SET);
	
	raw = malloc_safe(raw_sz + 1);
	
	if (fread(raw, 1, raw_sz, fp) != raw_sz)
		goto fail;
	
	raw[raw_sz] = '\0';
	
	fclose(fp);
	
	raw = preproc_oo_struct(raw);
	
	raw = preproc_remove_token(raw, "unsigned");
	raw = preproc_remove_token(raw, "const");
	
	fputs(raw, stdout);
	
	free(raw);
	
	return 0;
fail:
	if (fp)
		fclose(fp);
	if (raw)
		free(raw);
	return 1;
}
#endif
