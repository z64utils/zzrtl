#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "zzrtl.h" /* malloc_safe and die_i */

#if 0
#define die_i(x) { puts(x); exit(EXIT_FAILURE); }
#define malloc_safe malloc
#endif

#define die_i die

static
void *
memmem_stride(
	const void *l
	, size_t l_len
	, const void *s
	, size_t s_len
	, const size_t stride
)
{
	if (l == 0 || s == 0)
		return 0;
	register char *cur, *last;
	const char *cl = (const char *)l;
	const char *cs = (const char *)s;

	/* we need something to compare */
	if (l_len == 0 || s_len == 0)
		return 0;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return 0;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where its possible to find "s" in "l" */
	last = (char *)cl + l_len - s_len;

	for (cur = (char *)cl; cur <= last; cur+=stride)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return cur;

	return 0;
}

static
void
bytes2str(void *_src, void *_dst, int num)
{
	unsigned char *src = _src;
	char *dst = _dst;
	while (num)
	{
		sprintf(dst, "%02x", *src);
		src += 1;
		dst += 2;
		num -= 1;
	}
}

static
int
str2bytes(void *_src, void *_dst)
{
	char *src = _src;
	unsigned char *dst = _dst;
	char tc[4] = {0};
	int len = 0;
	
	while (*src)
	{
		int d;
		tc[0] = src[0];
		tc[1] = src[1];
		if (!sscanf(tc, "%02x", &d))
			break;
		*dst = d;
		src += 2;
		dst += 1;
		len += 1;
	}
	
	return len;
}

/* find byte string `ndl` inside raw data `hay`, with stride */
void *
find_bytes_stride
(void *_hay, int hay_sz, char *ndl, int stride, int only1)
{
	char *hay = _hay;
	int Ohay_sz = hay_sz;
	char *Ohay = _hay;
	static unsigned char *conv = 0;
	static char *Cconv = 0;
	int conv_max = 1024;
	int ndl_len;
	int wc;
	int ofs;
	
	if (!conv)
		conv = malloc_safe(conv_max / 2);
	
	if (!Cconv)
		Cconv = malloc_safe(conv_max + 8);
	
	/* runtime assertions */
	if (!hay || hay_sz <= 0 || !ndl)
		die_i("invalid find_bytes() arguments");
	
	/* stride assertion */
	if (stride <= 0)
		die_i("find_bytes() stride must be >= 1");
	
	ndl_len = strlen(ndl);
	
	/* uneven num characters */
	if (ndl_len & 1)
		die_i("find_bytes() ndl must contain even number of characters");
	
	/* too many characters */
	if (ndl_len >= conv_max)
		die_i("find_bytes() too many characters");
	
	/* invalid characters */
	if (strspn(ndl, "0123456789abcdefABCDEF*") != ndl_len)
		die_i("find_bytes() invalid characters");
#if 0 /* ndl_len & 1 already covers this */
	/* invalid num wildchars */
	if (strspn(ndl, "*") & 1)
		die_i("find_bytes() invalid number of wildcards");
#endif
	/* string is entirely wildcards */
	if (strspn(ndl, "*") == ndl_len)
		die_i("find_bytes() all characters are wildcards");
	
	/* determine if contains wildchar */
	for (wc = 0; ndl[wc]; ++wc)
		if (ndl[wc] == '*')
			break;
	if (wc == ndl_len)
		wc = -1;
	
	/* wc will now be != 0 if string contains wildchars */
	
	/* wildcard logic */
	if (wc >= 0)
	{
		char *found;
		int longest = 0;
		int longest_ofs = 0;
		int rem;
		int len;
		
		ofs = 0;
		
		/* get longest byte string */
		while (ofs < ndl_len)
		{
			int len = str2bytes(ndl + ofs, conv);
			
			/* skip wildcard byte */
			if (!len)
				ofs += 2;
			
			/* start offset of longest byte string */
			if (len > longest)
			{
				longest = len;
				longest_ofs = ofs;
			}
			
			ofs += len * 2;
		}
		ofs = longest_ofs;
		len = str2bytes(ndl + ofs, conv);
		
		/* offset everything b/c and shorter strings */
		ndl += ofs;
		hay += ofs / 2;
		hay_sz -= len * 2;
		rem = strlen(ndl) / 2;
#if 0
		fprintf(stderr, "ofs %x\n", ofs);
		fprintf(stderr, "len %x\n", len);
		fprintf(stderr, "hay = '%s'\n", hay);
		fprintf(stderr, "hay = '%.*s' (capped=%d)\n", hay_sz, hay, hay_sz);
		fprintf(stderr, "ndl = '%s'\n", ndl);
		fprintf(stderr, "cnv = '%s'\n", (char*)conv);
		fprintf(stderr, "cnv = '%.*s' (capped=%d)\n", len, (char*)conv, len);
#endif
		/* search haystack matching pattern */
		while (1)
		{
			int i;
			
			/* exceeded file boundary */
			if (hay >= Ohay + Ohay_sz)
				return 0;
			
			/* locate byte string in hay */
			found =
			memmem_stride(
				hay
				, hay_sz
				, conv
				, len
				, stride
			);
#if 0
			fprintf(stderr, "%p\n", found);
#endif
			/* no match found */
			if (!found)
				return 0;
			
			/* convert bytes to character string */
			bytes2str(found, Cconv, rem);
			
			/* apply wildcards */
			for (i = 0; Cconv[i]; ++i)
				if (ndl[i] == '*')
					Cconv[i] = '*';
#if 0
			fprintf(stderr, "'%s' vs '%s'\n", Cconv, ndl);
#endif
			/* it's a match */
			if (!strcasecmp(Cconv, ndl))
			{
				found = found - ofs / 2;
	
				/* if we don't allow block to contain multiple */
				if (only1 && ((found + stride) - Ohay) < Ohay_sz)
				{
					if (
						find_bytes_stride(
							found + stride
							, Ohay_sz - ((found + stride) - Ohay)
							, ndl
							, stride
							, 0
						)
					)
						die_i("find_bytes() found more than one '%s'", ndl);
				}
				
				return found;
			}
			
			/* keep searching */
			hay = found + stride;
			hay_sz = Ohay_sz - (hay - Ohay);
		}
	}
	
	/* no wildcards; go with fast method */
	else
	{
		char *ss;
		int len = str2bytes(ndl, conv);
		
		/* locate byte string in hay */
		ss =
		memmem_stride(
			hay
			, hay_sz
			, conv
			, len
			, stride
		);
	
		/* if we found one and we don't allow many,
			see if we can find any others */
		if (ss && only1 && ((ss + stride) - hay) < hay_sz)
		{
			if (
				memmem_stride(
					ss + stride
					, hay_sz - ((ss + stride) - hay)
					, conv
					, len
					, stride
				)
			)
				die_i("find_bytes() found more than one '%s'", ndl);
		}
		
		return ss;
	}
	
	return 0;
}

/* find byte string `ndl` inside raw data `hay` */
void *
find_bytes(void *_hay, int hay_sz, char *ndl, int only1)
{
	return find_bytes_stride(_hay, hay_sz, ndl, 1, only1);
}

/* find text string `ndl` inside raw data `hay`, with stride */
void *
find_text_stride
(void *_hay, int hay_sz, char *ndl, int stride, int only1)
{
	char *hay = _hay;
	char *ss;
	
	/* runtime assertions */
	if (!hay || hay_sz <= 0 || !ndl)
		die_i("invalid find_text() arguments");
	
	/* stride assertion */
	if (stride <= 0)
		die_i("find_text() stride must be >= 1");
	
	/* locate ndl in hay */
	ss =
	memmem_stride(
		hay
		, hay_sz
		, ndl
		, strlen(ndl)
		, stride
	);
	
	/* if we found one and we don't allow many,
	   see if we can find any others */
	if (ss && only1 && ((ss + stride) - hay) < hay_sz)
	{
		if (
			memmem_stride(
				ss + stride
				, hay_sz - ((ss + stride) - hay)
				, ndl
				, strlen(ndl)
				, stride
			)
		)
			die_i("find_text() found more than one '%s'", ndl);
	}
	
	return ss;
}

/* find text string `ndl` inside raw data `hay` */
void *
find_text(void *hay, int hay_sz, char *ndl, int only1)
{
	return find_text_stride(hay, hay_sz, ndl, 1, only1);
}

#ifdef FIND_BYTES_EXAMPLE
int main(void)
{
#if 0
	char hay[] = "00000000""\xde\xad""00""\xbe\xef";
	char *bs = "****dead****beef";
#else
	char hay[] =
	"0000!!0000000000000000000000!!00!!";
//	"    000000000000000000!!00!!";
	char *bs = "********2121****2121";
//	bs = "212130302121";
#endif
	char *loc;
	unsigned char dst[1024];
	int len;
	int i;
	len = str2bytes(bs, dst);
	
	loc = find_bytes_stride(hay, sizeof(hay), bs, 4);
	
	if (loc)
	{
		fprintf(stderr, "located at 0x%lX\n", loc - hay);
	}
	else
		fprintf(stderr, "didn't find anything\n");
#if 0
	for (i=0; i < len; ++i)
	{
		fprintf(stderr, "%02X\n", dst[i]);
	}
#endif
	return 0;
}
#endif

