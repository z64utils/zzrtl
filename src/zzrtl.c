#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* POSIX dependencies */
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* threading */
#include <pthread.h>

#include "zzrtl.h"
#include "n64texconv.h"   /* for texture format conversions     */
#include "stb/stb.h"      /* for png load/save and image resize */
#include "preproc.h"      /* string tokenization */
#include "enc/enc.h"      /* file compression    */
#include "enc/yar.h"      /* MM archive tools    */
#include "sha1.h"         /* sha1 checksum       */
#include "n64crc.h"       /* fix_crc */

/* TODO
 * @sauraen it would be really cool if you could add your compact yaz implementation as tinyyaz or something
 */

#include "wow.h"
#include "wow_dirent.h"
#undef   fopen
#undef   fread
#undef   fwrite
#define  fopen   wow_fopen
#define  fread   wow_fread
#define  fwrite  wow_fwrite


#define STRINGIZE(x) #x
#define STRINGIZE_(x) STRINGIZE(x)
#define S__LINE__ STRINGIZE_(__LINE__)

#define SIZE_16MB (1024 * 1024 * 16)
#define SIZE_4MB  (1024 * 1024 * 4)

#ifndef NDEBUG
//# define WANT_DEBUG_OUTPUT
static void print_sha1(void *data, int len)
{
	unsigned char checksum[64];
	char readable[64];
	stb_sha1(checksum, data, len);
	stb_sha1_readable(readable, checksum);
	
	fprintf(stderr, "%s\n", readable);
}

#define DBGLN	fprintf(stderr, __FILE__ S__LINE__ "\n");
#else
# define print_sha1(...) do { } while (0);
#define DBGLN
#endif

#define NWRITABLE (1 << 1)  /* rom_dma_queue() flag; will not update *
                             * `writable` of entry if used           */

#ifdef WANT_DEBUG_OUTPUT
# define DEBUG_PRINT(...) fprintf(stderr, __FILE__ "("S__LINE__"): " __VA_ARGS__)
#else
# define DEBUG_PRINT(...) do {} while (0)
#endif

static int g_die_no_waiting = 0;
static int g_use_cache = 1;
void die_no_waiting(int x) {
	g_die_no_waiting = x;
}
void zzrtl_use_cache(int on) {
	g_use_cache = on;
}

void
zzrtl_waitkey(void)
{
#ifdef _WIN32
	if (!g_die_no_waiting) {
		/* on windows, we wish to leave the window up afterwards */
		fprintf(stderr, "press enter key to exit");
		getchar();
	}
#endif
}

enum inject_mode
{
	INJECTMODE_DEFAULT = 0
	, INJECTMODE_LARGEST
	, INJECTMODE_EARLIEST
	, INJECTMODE_LATEST
};

enum mkstyle
{
	ZZRTL_MKSTYLE_PRE = 0
	, ZZRTL_MKSTYLE_PREX
	, ZZRTL_MKSTYLE_POST
	, ZZRTL_MKSTYLE_POSTX
};

enum str2valflags
{
	STR2VAL_ONLY_LAST = 1             /* no limit on num  */
	//, STR2VAL_WANT_FIRST = (1 << 1)   /* stop after first found */
	, STR2VAL_ONLY_FIRST = (1 << 2)   /* value must be at beginning */
	, STR2VAL_WANT_ERRS  = (1 << 3)   /* fatal errs if found != 1 */
};

/* global private storage should be fine */
static
struct
{
	char *dir_buf;          /* dir_mkname() buffer */
	enum mkstyle mkstyle;   /* dir_mkname() format style */
} g = {0};

static
int min_int(int a, int b)
{
	if (a < b)
		return a;
	return b;
}

struct compThread
{
	struct rom *rom;
	void *data;
	int (*encfunc)(
		void *src
		, unsigned int src_sz
		, void *dst
		, unsigned int *dst_sz
		, void *_ctx
	);
	char *enc;
	char *slash_enc;
	struct folder *list;
	int stride;   /* number of entries to advance each time */
	int ofs;      /* entry in list to start at */
	int report;   /* print info to stderr etc (last thread only) */
	void *ctx;    /* compression context */
	pthread_t pt; /* pthread */
};

/* types */

/* rom types */
struct writable
{
	struct writable  *next;      /* next known block (0 if none) */
	unsigned int      start;     /* start offset of block */
	unsigned int      len;       /* length of block */
	unsigned int      Ostart;    /* original start offset of block */
	unsigned int      Olen;      /* original length of block */
};

struct dmareg
{
	unsigned          offset;    /* offset at which to write   */
	struct dmareg    *next;      /* next in list               */
};

struct dma
{
	char             *compname;  /* name of compressed file    */
	unsigned char    *compbuf;   /* cache-less compressed data */
	unsigned int      index;     /* original index location    */
	unsigned int      forceIndex;/* force to exist at new index*/
	int               writable;  /* entry can be overwritten   */
	int               compress;  /* entry can be compressed    */
	int               deleted;   /* points to deleted file     */
	unsigned int      compSz;    /* cache-less compressed size */
	unsigned int      start;     /* start offset               */
	unsigned int      end;       /* end offset                 */
	unsigned int      Pstart;    /* start of physical (P) data */
	unsigned int      Pend;      /* end of physical (P) data   */
	unsigned int      Ostart;    /* original start/end read    */
	unsigned int      Oend;      /* from rom                   */
//	int               done;      /* done compressing file      */
	unsigned char     prefix[16];/* header data                */
	unsigned char     has_prefix;/* use header data            */
	struct dmareg    *reglist;   /* dma registration list      */
};

struct rom
{
	char             *fn;        /* filename of loaded rom            */
	unsigned char    *data;      /* raw rom data                      */
	unsigned int      data_sz;   /* size of rom data                  */
	unsigned int      ofs;       /* offset where write() writes       */
	unsigned int      fstart;    /* start offset where inject() wrote */
	unsigned int      fend;      /* end offset of what inject() wrote */
	unsigned char    *file;      /* pointer to where inject() wrote   */
	int               fdmaidx;   /* dma index inject() overwrote      */
	int               is_comp;   /* 1 if rom has been compressed      */
	struct writable  *writable;  /* linked list of writable blocks    */
	struct dma       *dma;       /* dma array                         */
	unsigned int      dma_num;   /* number of entries in dma array    */
	unsigned char    *dma_raw;   /* pointer to raw dmadata            */
	int               dma_ready; /* non-zero after dma_ready()        */
	unsigned int      dma_sz;    /* size of dmadata in rom            */
	int               align;     /* alignment                         */
	struct dma       *dma_last;  /* the last dma entry written        */
	int               injected;  /* num files injected                */
	
	/* injection prefix stuff */
	unsigned char     inject_prefix[16];
	int               has_injection_prefix;
	enum inject_mode  inject_mode;
	
	/* memory pools for things like compression*/
	struct
	{
		void *mb16;  /* 16 mb */
		void *mb4;   /* 4 mb  */
	} mem;
	
	struct
	{
		unsigned
			dmaext   : 1
			, minpad : 1
		;
	} flag;
};

/* folder types */
struct fldr_item
{
	char             *name;      /* name  */
	void             *udata;     /* udata */
	int               index;     /* index */
};

struct folder
{
	struct fldr_item *item;      /* item list */
	int               item_sz;   /* number of items alloc'd */
	struct fldr_item *active;    /* active item */
	int               remaining; /* remaining items in list */
	int               count;     /* items retrieved */
	int               max;       /* highest index */
};

/* conf types */
enum conf_fmt
{
	CONF_FMT_LIST = 0
	, CONF_FMT_TABLE
	, CONF_FMT_MAX /* r3 */
};

struct conf_item
{
	char             *name;      /* name  */
	char             *value;     /* value */
};

struct conf
{
	char             *raw;       /* raw data */
	char             *line;      /* active line */
	struct conf_item *item;      /* item list */
	struct conf_item *list_walk; /* r3 active item, when walking list */
	int               item_sz;   /* number of items alloc'd */
	enum conf_fmt     fmt;       /* format */
	int               remaining; /* non-zero if there is another line */
};

/****
 * internal use only functions
 ****/

/* internal error string formatting */
static
void
die_i(char *fmt, ...)
{
	va_list ap;
	
	fprintf(stderr, "[!] ");
	va_start (ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	
	fprintf(stderr, ", senpai! >///<\n");
	
	zzrtl_waitkey();
	
	exit(EXIT_FAILURE);
#if 0
	fprintf(stderr, "[!] %s, senpai! >///<\n");
	exit(EXIT_FAILURE);
#endif
}

#if 0
/* test if path is a directory */
static
int
isdir(const char *path)
{
	struct stat buf;
	if (stat(path, &buf) != 0)
		return 0;
	return S_ISDIR(buf.st_mode);
}
#endif

/* abs (int) */
static
int
abs_int(int x)
{
	if (x < 0)
		return -x;
	
	return x;
}

/* safe calloc */
void *
calloc_safe(size_t nmemb, size_t size)
{
	void *result;
	
	result = calloc(nmemb, size);
	
	if (!result)
		die_i("allocation failure");
	
	return result;
}

/* safe malloc */
void *
malloc_safe(size_t size)
{
	void *result;
	
	result = malloc(size);
	
	if (!result)
		die_i("allocation failure");
	
	return result;
}

/* safe realloc */
void *
realloc_safe(void *ptr, size_t size)
{
	void *result;
	
	result = realloc(ptr, size);
	
	if (!result)
		die_i("allocation failure");
	
	return result;
}

/* safe memdup */
static
void *
memdup_safe(void *mem, size_t size)
{
	assert(mem);
	
	void *ret = malloc_safe(size);
	
	memcpy(ret, mem, size);
	
	return ret;
}

/* safe strdup */
static
char *
strdup_safe(char *str)
{
	char *result;
	
	assert(str);
	
	result = strdup(str);
	
	if (!result)
		die_i("allocation failure");
	
	return result;
}

/* convert a string to lowercase */
static
void
tolower_string(char *str)
{
	assert(str);
	for ( ; *str; ++str)
		*str = tolower(*str);
}

#if 0
/* make a directory */
static
int
wrapper_mkdir(const char *path)
{
#ifdef _WIN32
	return mkdir(path);
#else
	return mkdir(path, 0777);
#endif
}
#endif

static
void *
writable_free(struct writable *wr)
{
	while (wr)
	{
		struct writable *next;		
		next = wr->next;		
		free(wr);		
		wr = next;
	}
	
	return 0;
}

/* sort struct dma array by ascending index */
static
int
dma_sort_index_ascending(const void *_a, const void *_b)
{
	const struct dma *a = _a;
	const struct dma *b = _b;
	
	if (a->index < b->index)
		return -1;
	else if (a->index > b->index)
		return 1;
	return 0;
	
	//return a->index > b->index;
}

/* sort struct dma array by ascending start */
static
int
dma_sort_start_ascending(const void *_a, const void *_b)
{
	const struct dma *a = _a;
	const struct dma *b = _b;
	
	if (a->start < b->start)
		return -1;
	else if (a->start > b->start)
		return 1;
	return 0;
	
	//return a->start > b->start;
}

/* sort struct dma array by descending size */
static
int
dma_sort_size_descending(const void *_a, const void *_b)
{
	const struct dma *a = _a;
	const struct dma *b = _b;
	
	unsigned int a_len = a->end - a->start;
	unsigned int b_len = b->end - b->start;
	
	if (a_len < b_len)
		return 1;
	else if (a_len > b_len)
		return -1;
	return 0;
	
	//return (a->end - a->start) < (b->end - b->start);
}

/* sort struct dma array by descending compSz */
static
int
dma_sort_compSz_descending(const void *_a, const void *_b)
{
	const struct dma *a = _a;
	const struct dma *b = _b;
	
	if (a->compSz < b->compSz)
		return 1;
	else if (a->compSz > b->compSz)
		return -1;
	return 0;
	
	//return (a->end - a->start) < (b->end - b->start);
}

/* sort struct dma array by ascending compSz */
static
int
dma_sort_compSz_ascending(const void *_a, const void *_b)
{
	const struct dma *a = _a;
	const struct dma *b = _b;
	
	if (a->compSz < b->compSz)
		return -1;
	else if (a->compSz > b->compSz)
		return 1;
	return 0;
	
	//return (a->end - a->start) < (b->end - b->start);
}

static
void
display_writable_regions(struct rom *rom)
{
#ifndef NDEBUG
	if (rom->writable)
	{
		struct writable *wr;
		unsigned int sum = 0;
		
		/* debugging help */
		fprintf(stderr, "rom_dma_ready() completed, writable regions:\n");
		for (wr = rom->writable; wr; wr = wr->next)
		{
			fprintf(stderr,
				"%08X len %08X\n"
				, wr->start
				, wr->len
			);
			sum += wr->len;
		}
		
		fprintf(stderr, " sum: %08X\n", sum);
	}
#endif
}

#if 0
/* sort struct dma array by ascending start (unused entries at end) */
static
int
dma_sort_start_ascending_0end(const void *_a, const void *_b)
{
	const struct dma *a = _a;
	const struct dma *b = _b;
	
	/* unused entries go at end of list */
	if (b->start == 0 && b->end == 0)
		return -1;
	
	return a->start > b->start;
}
#endif

/* mark data range within rom as writable; files
   injected with inject() will use this space */
static
void
rom_writable(struct rom *rom, unsigned int start, unsigned int end)
{
	struct writable *wr;
	
	wr = malloc_safe(sizeof(*wr));
	
	if (end <= start)
		die_i(
			"how exactly is the region from 0x%X to 0x%X writable"
			, start
			, end
		);
	
	wr->start = start;
	wr->len = end - start;
	wr->next = 0;
	
	/* back up original values */
	wr->Ostart = wr->start;
	wr->Olen = wr->len;
	
	/* link into linked list */
	if (!rom->writable)
	{
		rom->writable = wr;
	}
	else
	{
		struct writable *last;
		
		for (last = rom->writable; last->next; last = last->next)
			do{}while(0);
		
		last->next = wr;
	}
}

/* zero-initialize writable regions */
static
void
rom_writable_zero(struct rom *rom)
{
	struct writable *wr;
	
	assert(rom);
	assert(rom->writable);
	
	for (wr = rom->writable; wr; wr = wr->next)
	{
		/* zero-initialize */
		memset(rom->data + wr->start, 0, wr->len);
	}
}

/* write dma table into rom */
static
void
rom_write_dmadata(struct rom *rom)
{
	struct dma *dma;
	int count;
	int countUsed;
	unsigned int oofs;
	
	assert(rom);
	assert(rom->dma);
	
	/* do not write dmadata */
	if (!rom->dma_raw)
		return;
	
	dma = rom->dma;
	count = rom->dma_num;
	
/* sort by start offset, ascending, with unused entries at end */
	/* first sort by size, descending */
	qsort(
		dma
		, count
		, sizeof(*dma)
		, dma_sort_size_descending
	);
	/* now find the first entry where size == 0 */
	for (dma = rom->dma; dma - rom->dma < count; ++dma)
		if (dma->start == dma->end)
			break;
	
	countUsed = dma - rom->dma;
	/* now sort all the ones before it, ascending start offset */
	qsort(
		rom->dma
		, countUsed
		, sizeof(*dma)
		, dma_sort_start_ascending
	);
	
#if 0	/* try sorting to original index */
	qsort(
		rom->dma
		, countUsed
		, sizeof(*dma)
		, dma_sort_index_ascending
	);
#endif
	
	/* r5: reorder such that index is preserved on files
	 *     the rom.file_dma() function was used on
	 */
	for (dma = rom->dma; dma - rom->dma < countUsed; ++dma)
	{
		if (dma->forceIndex)
			dma->index = dma->forceIndex - 1;
		else
			dma->index = dma - rom->dma;
	}
	qsort(
		rom->dma
		, countUsed
		, sizeof(*dma)
		, dma_sort_index_ascending
	);
	/* extra pass: overlapping indices get moved along */
	for (dma = rom->dma; dma - rom->dma < countUsed; ++dma)
	{
		struct dma *tmp;
		unsigned force;
		
		/* skip entries that aren't applicable */
		if (!dma->forceIndex)
			continue;
		
		force = dma->forceIndex - 1;
		
		/* prev entry */
		if (dma != rom->dma
			&& (tmp = dma - 1)
			&& tmp->index == force
		)
			tmp->index = countUsed;
		
		/* next entry */
		if ((dma - rom->dma) < countUsed - 1
			&& (tmp = dma + 1)
			&& tmp->index == force
		)
			tmp->index = countUsed;
	}
	qsort(
		rom->dma
		, countUsed
		, sizeof(*dma)
		, dma_sort_index_ascending
	);
	/* error checking pass */
	for (dma = rom->dma; dma - rom->dma < countUsed; ++dma)
	{
		if (dma->forceIndex
			&& (dma - rom->dma) != (dma->forceIndex - 1)
		)
			die_i("something went wrong when sorting dma"
				" (%d != %d)"
				, (int)(dma - rom->dma)
				, dma->forceIndex - 1
			);
	}
	
	/* back up offset */
	oofs = rom->ofs;
	rom->ofs = rom->dma_raw - rom->data;
	
	int rem = rom->dma_sz;
	
	/* the first 32 bytes are reserved */
	if (rom->flag.dmaext)
	{
		/* the game has a sanity check at the beginning */
		rom_write32(rom, 0x00000000);
		rom_write32(rom, 0x00001060);
		rom->ofs = rom->dma_raw - rom->data;
		rem -= 32;
		rom->ofs += 32;
	}
	
	/* write every entry */
	for (dma = rom->dma; dma - rom->dma < count; ++dma)
	{
		int sz;
		
		/* if rom not compressed, walk dma->registers */
		if (!rom->is_comp)
		{
			struct dmareg *reg;
			unsigned Oofs = rom->ofs;
			
			for (reg = dma->reglist; reg; reg = reg->next)
			{
				rom->ofs = reg->offset;
				rom_write16(rom, dma - rom->dma);
			}
			rom->ofs = Oofs;
		}
		
		/* r4: shiny new dmadata format */
		if (rom->flag.dmaext)
		{
			/* XXX careful, start offsets must account for prefixes */
			
			/* TODO dmaext */
			unsigned Vstart;
			unsigned Vend;
			unsigned Pstart;
			unsigned Pend;
			unsigned cleanPstart;
			int overlap = 0;
			
			Vstart = dma->start;
			Vend   = dma->end;
			Pstart = dma->Pstart;
			Pend   = dma->Pend;
			cleanPstart = Pstart;
			
			enum {
				DMAEXT_COMPRESS = 1 << 31
				, DMAEXT_OVERLAP = 1 << 0
				, DMAEXT_PREFIX  = 1 << 1
			};
			
			if (dma->has_prefix)
			{
				Vstart -= dma->has_prefix;
				Pstart -= dma->has_prefix;
				cleanPstart -= dma->has_prefix;
				Pstart |= DMAEXT_PREFIX;
			}
			
			/* compressed file */
			if (Pend > Pstart)
			{
				/* overwrite first word with compressed size */
				unsigned t = rom_tell(rom);
				rom_seek(rom, Pstart);
				rom_write32(rom, Pend - Pstart);
				rom_seek(rom, t);
				Pstart |= DMAEXT_COMPRESS;
			}
		
			/* r4: don't forget file prefixes */
			if (dma->has_prefix)
			{
				unsigned char *dst = rom->data;
				dst += cleanPstart;
				memcpy(dst, dma->prefix, dma->has_prefix);
			}
			
			/* if (this->vend == next->vstart) overlap (note prefixes) */
			unsigned next_Vstart = dma[1].start - dma[1].has_prefix;
			overlap = next_Vstart == Vend;
			
			sz = 12;
			if (overlap)
			{
				sz = 8;
				Pstart |= DMAEXT_OVERLAP;
			}
			
			rom_write32(rom, Vstart);
			rom_write32(rom, Pstart);
			
			/* full size, including end */
			if (sz == 12)
				rom_write32(rom, Vend);
		}
		
		/* original dmadata format */
		else
		{
			rom_write32(rom, dma->start);
			rom_write32(rom, dma->end);
			rom_write32(rom, dma->Pstart);
			rom_write32(rom, dma->Pend);
			sz = 16;
		}
		
		/* r4: new end condition: all entries written */
		if (!dma->end)
			break;
		
		/* no more room for entries */
		if (sz < 12) sz = 12; /* the last entry must be at least 12 */
		if (rem <= sz)
			die_i("i ran out of dma space");
		
		rem -= sz;
	}
	
	/* restore offset */
	rom->ofs = oofs;
}

/* get a writable dma entry from a rom */
static
struct dma *
rom_get_dma(struct rom *rom, int comp)
{
	struct dma *dma;
	
	assert(rom);
	assert(rom->dma);
	
	for (dma = rom->dma; dma - rom->dma < rom->dma_num; ++dma)
	{
		if (dma->writable)
		{
			dma->writable = 0;
			dma->compress = !!comp;
			rom->fdmaidx = dma - rom->dma;
			return dma;
		}
	}
	
	die_i(
		"i ran out of dma entries (you will have to delete files, "
		"mark more dma entries as writable, or expand dmadata)"
	);
	
	return dma;
}

/* strip quotes from a string */
static
void
strip_quotes(char **_str, int sz)
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

/* contiguous block test function for use with preproc_usecontig() */
static
int
iscontig(char *str)
{
	int len = 0;
	
	while (*str && isgraph(*str) && !isspace(*str))
	{
		len += 1;
		str += 1;
	}
	
	return len;
}

/* contiguous block testing function which separates integers *
 * (including hexadecimal) from standard characters           */
static
int
iscontig_int_search(char *str)
{
	int len = 0;
	int is_num = 0;
	int is_hex = 0;
	
	DEBUG_PRINT("int_search(%s)\n", str);
	
	/* skip "0x" */
	if (*str == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		len += 2;
		str += 2;
		is_num = 1;
		is_hex = 1;
	}
	
	/* skip preceding '-' if there is one */
	else if (*str == '-')
	{
		len += 1;
		str += 1;
		is_num = 1;
	}
	
	/* decimal number */
	else if (isdigit(*str))
		is_num = 1;
	
	while (*str)
	{
		/* contiguous digits */
		if (is_num)
		{
			/* hexadecimal */
			if (is_hex && !isxdigit(*str))
				break;
			/* decimal */
			else if(!is_hex && !isdigit(*str))
				break;
		}
		
		/* contiguous non-numbers */
		else
		{
			/* found a digit */
			if (isdigit(*str))
				break;
			
			/* negative number */
			if (isdigit(str[1]) && *str == '-')
				break;
		}
		
		/* advance to next character */
		len += 1;
		str += 1;
	}
	
	return len;
}

/* locate conf item by name */
static
struct conf_item *
conf_item_find(struct conf *conf, char *name)
{
	struct conf_item *item;
	
//	if (!name)
//		die_i("i can't search conf for uninitialized string");
	if (!conf || !conf->item)
	{
		if (name)
			die_i("i can't search for '%s' in uninitialized conf", name);
		else
			die_i("i can't search for '0' in uninitialized conf");
	}
	
	/* active item's value */
	if (!name)
	{
		if (!conf->remaining)
			return 0;
		
		return conf->list_walk;
	}
	
	/* walk through item list */
	for (
		item = conf->item
		; item - conf->item < conf->item_sz
		; ++item
	)
		/* if names match, this is the item we want */
		if (item->name && !strcasecmp(name, item->name))
			return item;
	
	return 0;
}

/* locate conf item by name, throw error on failure */
static
struct conf_item *
conf_item_find_error(struct conf *conf, char *name)
{
	struct conf_item *item;
	
	item = conf_item_find(conf, name);
	
	if (!item || !item->value)
		die_i("i couldn't locate '%s' in conf", name);
	
	return item;
}

#if 0
/* get number of characters of a number */
static
int
str_int_len(char *str, int is_hex)
{
	int len = 0;
	
	/* skip "0x" */
	if (is_hex)
	{
		len += 2;
		str += 2;
	}
	
	/* skip preceding '-' if there is one */
	else if (*str == '-')
	{
		len += 1;
		str += 1;
	}
	
	/* test every character */
	while (*str)
	{
		/* hexadecimal */
		if (is_hex && !isxdigit(*str))
			break;
		
		/* decimal */
		else if (!isdigit(*str))
			break;
		
		/* advance */
		str += 1;
		len += 1;
	}
	
	return len;
}
#endif

/* tokenizes a string and retrieves whatever integers it can find */
static
int
str2val(char *_str, int *dst, enum str2valflags flags)
{
	void *o_contig;
	char *str = _str;
	int retrieved = 0;
	int len;
	
	assert(str);
	assert(dst);
	
	/* back up contig with preproc_getcontig() */
	o_contig = preproc_getcontig();
	
	/* override with custom contig test */
	preproc_usecontig(iscontig_int_search);
	
	while (*str)
	{
		/* get length of current token */
		len = preproc_toklen(str);
		
		DEBUG_PRINT(" > '%.*s' (%d chars)\n", len, str, len);
		
		/* negative decimal value */
		if (*str == '-' && isdigit(str[1]))
		{
			/* failed to retrieve value */
			if (!sscanf(str, "%d", dst))
				die_i("couldn't retrieve value from string '%s'", _str);
#if 0
			/* skip number only, as token may contain more numbers */
			len = str_int_len(str, 0);
#endif
			retrieved += 1;
		}
		
		/* is 0 - 9 */
		else if (isdigit(*str))
		{
			/* hexadecimal ("0x") */
			if (*str == '0' && tolower(str[1]) == 'x')
			{
				/* failed to retrieve value */
				if (!isxdigit(str[2]) || !sscanf(str, "0x%X", dst))
					die_i("couldn't retrieve value from string '%s'", _str);
#if 0
				/* skip number only, as token may contain more numbers */
				len = str_int_len(str, 1);
#endif
			}
			
			/* decimal */
			else
			{
				/* failed to retrieve value */
				if (!sscanf(str, "%d", dst))
					die_i("couldn't retrieve value from string '%s'", _str);
#if 0
				/* skip number only, as token may contain more numbers */
				len = str_int_len(str, 0);
#endif
			}
			
			retrieved += 1;
		}
		
		/* user wants to parse only first token */
		if (flags & STR2VAL_ONLY_FIRST)
			break;
		
		str += len;
	}
	
	/* restore previous contig test */
	preproc_usecontig(o_contig);
	
	/* fatal error conditions */
	if (flags & STR2VAL_WANT_ERRS)
	{
		if (retrieved < 1)
			die_i("failed to retrieve value from '%s'", _str);
		if (retrieved > 1 && !(flags & STR2VAL_ONLY_LAST))
			die_i("'%s' contains too many unique values", _str);
	}
	
	return retrieved;
}

/* load a file into an existing buffer */
static
void *
file_load_into(char *fn, unsigned int *sz, void *dst)
{
	FILE *fp;
	
	assert(fn);
	assert(sz);
	assert(dst);
	
	*sz = 0;
	
	fp = fopen(fn, "rb");
	if (!fp)
		return 0;
	
	fseek(fp, 0, SEEK_END);
	*sz = ftell(fp);
	
	if (!*sz)
		goto fail_close;
	
	fseek(fp, 0, SEEK_SET);
	
	if (fread(dst, 1, *sz, fp) != *sz)
		goto fail_close;
	
	fclose(fp);
	
	return dst;
	
fail_close:
	fclose(fp);
	return 0;
}

/* load a file */
static
void *
file_load(char *fn, unsigned int *sz, int pad)
{
	FILE *fp;
	unsigned char *dst;
	
	assert(fn);
	assert(sz);
	
	*sz = 0;
	
	fp = fopen(fn, "rb");
	if (!fp)
		return 0;
	
	fseek(fp, 0, SEEK_END);
	*sz = ftell(fp);
	
	if (!*sz)
		goto fail_close;
	
	fseek(fp, 0, SEEK_SET);
	
	dst = malloc_safe(*sz + pad);
	
	if (fread(dst, 1, *sz, fp) != *sz)
		goto fail_close_free;
	
	fclose(fp);
	
	/* r3: guarantee end padding is zero-initialized */
	if (pad > 0)
		memset(dst + *sz, 0, pad);
	
	return dst;
	
fail_close_free:
	free(dst);
fail_close:
	fclose(fp);
	return 0;
}

/* write file */
static
unsigned int
file_write(char *fn, void *data, unsigned int data_sz)
{
	FILE *fp;
	
	assert(fn);
	assert(data);
	assert(data_sz);
	
	fp = fopen(fn, "wb");
	if (!fp)
		return 0;
	
	data_sz = fwrite(data, 1, data_sz, fp);
	
	fclose(fp);
	
	return data_sz;
}

/* get dma entry by index (useful after rearrangement happens) */
static
struct dma *
dma_get_idx(struct rom *rom, int idx)
{
	struct dma *dma;
	/* walk dma list for matching index */
	for (dma = rom->dma; dma - rom->dma < rom->dma_num; ++dma)
	{
		if (dma->index == idx)
			break;
	}
	
	/* get region where this file will fit */
	if (dma - rom->dma >= rom->dma_num)
		die_i("dma index too high (%d > %d)", dma - rom->dma, rom->dma_num);
	
	return dma;
}

/* locate a region in which file of specified size can fit;
   this function will always return a valid region, as it will
   throw a fatal error if it fails to find one */
static
struct writable *
rom_get_region(struct rom *rom, unsigned int sz)
{
	struct writable *smallest = 0;
	struct writable *largest = 0;
	struct writable *earliest = 0;
	struct writable *latest = 0;
	struct writable *wr;
	
	assert(rom);
	
	if (!rom->writable)
		die_i("you haven't specified writable rom regions");
	
	for (wr = rom->writable; wr; wr = wr->next)
	{
		int align;
		
		/* skip empty regions */
		if (!wr->len)
			continue;
		
		/* if start is not aligned to rom->align, align it */
		align = wr->start & (rom->align - 1);
		if (align)
		{
			align = rom->align - align;
			
			/* alignment uses up remaining space in region */
			if (align >= wr->len)
			{
				wr->len = 0;
				continue;
			}
			
			/* offset alignment */
			wr->start += align;
			wr->len -= align;
		}
		
		/* if region contains enough space to fit the file */
		if (wr->len >= sz)
		{
			//return wr;
			if (!smallest)
				smallest = wr;
			if (!largest)
				largest = wr;
			if (!earliest)
				earliest = wr;
			if (!latest)
				latest = wr;
			
			if (wr->len < smallest->len)
				smallest = wr;
			if (wr->len > largest->len)
				largest = wr;
			if (wr->start < earliest->start)
				earliest = wr;
			if (wr->start > latest->start)
				latest = wr;
		}
	}
	
	if (!smallest)
	{
		display_writable_regions(rom);
		die_i("i ran out of space");
	}
	
	switch (rom->inject_mode)
	{
		case INJECTMODE_DEFAULT:
		default:
			return smallest;
			break;
		
		case INJECTMODE_EARLIEST:
			return earliest;
			break;
		
		case INJECTMODE_LARGEST:
			return largest;
			break;
		
		case INJECTMODE_LATEST:
			return latest;
	}
	
	return smallest;
}

/* check if file has extension */
char *
is_ext(char *fn, char *ext)
{
	assert(fn);
	assert(ext);
	
	/* jump to period at end */
	fn = strrchr(fn, '.');
	
	/* no period found, so no extension */
	if (!fn)
		return 0;
	
	char *slash;
	slash = strrchr(fn, '/');
	if (!slash)
		slash = strrchr(fn, '\\');
	if (!slash)
		slash = fn;
	
	/* the period we found is part of folder above the current name */
	if (slash > fn)
		return 0;
	
	/* skip period */
	fn += 1;
	
	/* if extensions aren't the same length, don't compare them */
	if (strlen(fn) != strlen(ext))
		return 0;
	
	/* return comparison result */
	if (strcasecmp(fn, ext))
		return 0;
	
	/* return pointer to extension substring */
	return fn;
}

/* if provided string of format "*.ext", searches working directory
   for file of that extension; will throw fatal error if it finds
   too many; if it finds none, it will return "ext.ext" */
static
char *
find_extension(char *fn, int must_exist)
{
	wow_DIR *dir;
	struct wow_dirent *ep;
	static char *buf = 0;
	int buf_max = 4096;
	int count = 0;
	
	//assert(fn);
	if (!fn)
		die_i("filename == 0");
	
	/* does not match format "*.ext" */
	if (fn[0] != '*' || fn[1] != '.' || !fn[2])
		return fn;
	
	if (!buf)
		buf = malloc_safe(buf_max);
	
	/* skip the "*." part */
	fn += 2;
	
	/* parse directory */
	dir = wow_opendir(".");
	if (!dir)
		die_i("failed to parse directory '.'");
	while ((ep = wow_readdir(dir)))
	{
		char *dn;
		char *ss;
		
		dn = (char*)ep->d_name;
		
		/* skip directories */
		if (wow_is_dir(dn))
			continue;
		
		/* skip anything starting with '.' */
		if (*dn == '.')
			continue;
		
		/* find last '.' in string */
		ss = strrchr(dn, '.');
		if (!ss)
			continue;
		
		/* skip if extensions don't match */
		if (strcasecmp(fn, ss + 1))
			continue;
		
		/* extensions match; copy full name to buffer, note finding */
		strcpy(buf, dn);
		count += 1;
	}
	wow_closedir(dir);
	
	if (count < 1)
	{
		/* fatal error if failed to locate */
		if (must_exist)
			die_i(
				"i failed to locate a file with the extension '%s'"
				, fn
			);
		/* ext.ext */
		sprintf(buf, "%s.%s", fn, fn);
	}
	if (count > 1)
		die_i(
			"i found too many files with the extension '%s'"
			", idk which to use"
			, fn
		);
	
	return buf;
}

/* get size of a file; returns 0 if fopen fails */
static
unsigned int
file_size(char *fn)
{
	FILE *fp;
	unsigned int sz;
	
	fp = fopen(fn, "rb");
	if (!fp)
		return 0;
	
	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fclose(fp);
	
	return sz;
}

/* inject raw data into rom, returning a pointer to the result;
   it also propagates start and end offsets with result, or 0;
   if a pointer to 0 or a size of 0 is specified, it returns 0 */
void *
rom_inject_raw(struct rom *rom, void *raw, unsigned int sz, int comp)
{
	struct writable *wr;
	struct dma *dma;
	unsigned char *dst;
	unsigned int sz16;
	
	if (!rom || !rom->data)
		die_i("cannot inject into uninitialized rom");
	
	/* update these for if user wants them, and return early */
	if (!raw || !sz)
	{
		rom->fstart = 0;
		rom->fend = 0;
		rom->file = 0;
		return 0;
	}
	
	/* align 16 */
	sz16 = sz;
	if (sz16 & 15)
		sz16 += 16 - (sz16 & 15);
	
	/* increase size by 16 bytes if prefix requested */
	sz16 += rom->has_injection_prefix;
	
	/* get region where this file will fit */
	wr = rom_get_region(rom, sz16);
	
	/* note offsets for getters */
	rom->fstart = wr->start + rom->has_injection_prefix;
	rom->fend = wr->start + sz16;
	rom->file = rom->data + rom->fstart;
	
	/* copy into rom */
	dst = rom->file;
	memcpy(dst, raw, sz);
	
	/* add file to dmadata */
	dma = rom_get_dma(rom, comp);
	rom->dma_last = dma;
	dma->start = rom->fstart;
	dma->Pstart = dma->start;
	dma->end = rom->fend;
	dma->deleted = 0;
	if (rom->has_injection_prefix)
	{
		memcpy(
			dst - rom->has_injection_prefix
			, rom->inject_prefix
			, rom->has_injection_prefix
		);
		memcpy(dma->prefix, rom->inject_prefix, rom->has_injection_prefix);
		dma->has_prefix = rom->has_injection_prefix;
		rom->has_injection_prefix = 0;
	}
	
	/* update writable region */
	wr->start += sz16;
	wr->len -= sz16;
	
	/* return pointer to copied data inside rom */
	return dst;
}

/* inject data over existing dma entry (file size must match) */
void *
rom_inject_raw_dma(struct rom *rom, void *raw, unsigned int sz, int comp, int idx)
{
	struct writable *wr=0, wr_ = {0};
	struct dma *dma;
	unsigned char *dst;
	unsigned int sz16;
	
	if (!rom || !rom->data)
		die_i("cannot inject into uninitialized rom");
	
	if (!raw)
		die_i("rom.inject_raw_dma(%d): raw == 0\n", idx);
	
	if (!rom->dma || !rom->dma_num)
		die_i("cannot inject via dma until rom dma initialized");
	
	/* update these for if user wants them, and return early */
	if (!raw || !sz)
	{
		rom->fstart = 0;
		rom->fend = 0;
		rom->file = 0;
		return 0;
	}
	
	/* align 16 */
	sz16 = sz;
	if (sz16 & 15)
		sz16 += 16 - (sz16 & 15);
	
	/* get region where this file will fit */
	if (idx >= rom->dma_num)
		die_i("dma injection too high (%d > %d)", idx, rom->dma_num);
	dma = dma_get_idx(rom, idx);
	rom->dma_last = dma;
	dma->deleted = 0;
	wr_ = (struct writable)
	{
		.start = dma->Ostart
		, .len = dma->Oend - dma->Ostart
		, .Ostart = dma->Ostart
		, .Olen = dma->Oend - dma->Ostart
	};
	wr = &wr_;
	
	/* make sure size is valid */
	if (sz != wr->len)
		die_i(
			"rom.inject_dma(%d) unmatching size (%08X != %08X)\n"
			, idx
			, sz
			, wr->len
		);
	
	/* copy into rom */
	dst = rom->data + wr->start;
	memcpy(dst, raw, sz);
	
	/* note offsets for getters */
	rom->fstart = wr->start;
	rom->fend = wr->start + sz16;
	rom->file = dst;
	rom->injected++;
	
	/* return pointer to copied data inside rom */
	return dst;
}

/* inject raw data into rom, returning a pointer to the result;
   it also propagates start and end offsets with result, or 0;
   if a pointer to 0 or a size of 0 is specified, it returns 0 */
static
void *
rom_inject_file(struct rom *rom, char *fn, int comp)
{
	struct writable *wr;
	struct dma *dma;
	unsigned char *dst;
	unsigned int sz;
	unsigned int sz16;
	
	/* if fn starts with '*', grab by extension */
	fn = find_extension(fn, 0);
	
	if (!rom || !rom->data)
		die_i("cannot inject into uninitialized rom");
	
	sz = file_size(fn);
	
	/* update these for if user wants them, and return early */
	if (!sz)
	{
		rom->fstart = 0;
		rom->fend = 0;
		rom->file = 0;
		return 0;
	}
	
	/* at this point, we know file exists, so any issues
	   with file_load_into() will be due to a read error */
	
	/* align 16 */
	sz16 = sz;
	if (sz16 & 15)
		sz16 += 16 - (sz16 & 15);
	
	/* increase size by 16 bytes if prefix requested */
	sz16 += rom->has_injection_prefix;
	
	/* get region where this file will fit */
	wr = rom_get_region(rom, sz16);
	
	/* note offsets for getters */
	//fprintf(stdout, "%08X : wrote '%s'\n", wr->start, fn);
	rom->fstart = wr->start + rom->has_injection_prefix;
	rom->fend = wr->start + sz16;
	rom->file = rom->data + rom->fstart;
	
	/* get rom pointer */
	dst = rom->file;
	
	/* load file into rom at offset */
	if (!file_load_into(fn, &sz, dst))
		die_i("file read error");
	
	/* add file to dmadata */
	dma = rom_get_dma(rom, comp);
	rom->dma_last = dma;
	dma->start = rom->fstart;
	dma->Pstart = dma->start;
	dma->end = rom->fend;
	dma->deleted = 0;
	if (rom->has_injection_prefix)
	{
		memcpy(
			dst - rom->has_injection_prefix
			, rom->inject_prefix
			, rom->has_injection_prefix
		);
		memcpy(dma->prefix, rom->inject_prefix, rom->has_injection_prefix);
		dma->has_prefix = rom->has_injection_prefix;
		rom->has_injection_prefix = 0;
	}
	
	/* update writable region */
	wr->start += sz16;
	wr->len -= sz16;
	
	/* return pointer to loaded data inside rom */
	return dst;
}

/* inject file over existing dma entry (file size must match) */
void *
rom_inject_dma(struct rom *rom, char *fn, int comp, int idx)
{
	struct writable *wr, wr_;
	struct dma *dma;
	unsigned char *dst;
	unsigned int sz;
	unsigned int sz16;
	
	/* if fn starts with '*', grab by extension */
	fn = find_extension(fn, 0);
	
	if (!rom || !rom->data)
		die_i("cannot inject into uninitialized rom");
	
	if (!fn)
		die_i("rom.inject_dma(%d) no filename provided", idx);
	
	if (!rom->dma || !rom->dma_num)
		die_i("cannot inject via dma until rom dma initialized");
	
	sz = file_size(fn);
	
	/* update these for if user wants them, and return early */
	if (!sz)
	{
		rom->fstart = 0;
		rom->fend = 0;
		rom->file = 0;
		return 0;
	}
	
	/* at this point, we know file exists, so any issues
	   with file_load_into() will be due to a read error */
	
	/* align 16 */
	sz16 = sz;
	if (sz16 & 15)
		sz16 += 16 - (sz16 & 15);
	
	dma = dma_get_idx(rom, idx);
	dma->deleted = 0;
	rom->dma_last = dma;
	wr_ = (struct writable)
	{
		.start = dma->Ostart
		, .len = dma->Oend - dma->Ostart
		, .Ostart = dma->Ostart
		, .Olen = dma->Oend - dma->Ostart
	};
	wr = &wr_;
	
	/* make sure size is valid */
	if (sz != wr->len)
		die_i(
			"rom.inject_dma(%d) unmatching size (%08X != %08X)\n"
			, idx
			, sz
			, wr->len
		);
	
	/* get rom pointer */
	dst = rom->data + wr->start;
	
	/* note offsets for getters */
	//fprintf(stdout, "%08X : wrote '%s'\n", wr->start, fn);
	rom->fstart = wr->start;
	rom->fend = wr->start + sz16;
	rom->file = dst;
	rom->injected++;
	
	/* load file into rom at offset */
	dst = file_load_into(fn, &sz, dst);
	if (!dst)
		die_i("file read error");
	
	/* return pointer to loaded data inside rom */
	return dst;
}

/****
 * generic functions
 ****/

/* terminate with a custom error message */
void
die(char *fmt, ...)
{
	va_list ap;
	
	if (fmt)
	{
		va_start (ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		
		fprintf(stderr, "\n");
	}
	
	zzrtl_waitkey();
	
	exit(EXIT_FAILURE);
}

/* load a file */
void *
loadfile(char *fn, int *sz, int optional)
{
	unsigned int Nsz;
	void *data;
	
	if (!fn)
		die_i("loadfile no filename given");
	
	data = file_load(fn, &Nsz, 16);
	if (!data && !optional)
		die_i("loadfile failed to load '%s'", fn);
	
	if (sz)
		*sz = Nsz;
	return data;
}

/* extract tsv string */
char *
tsv_col_row(char *tsv, char *colname, int row)
{
	if (!tsv)
		return 0;
	
	if (!colname)
		return 0;
	
	if (row < 0)
		return 0;
	
	char *Fnl;        /* pointer to first newline */
	char *out;        /* output string */
	char *ss;         /* generic substring */
	int   cols;       /* columns */
	int   col = -1;   /* index of desired column */
	int   rem;        /* remaining \t, (\n && \r) chars til item */
	static struct {
		char str[512];
	} *str_stack = 0;
	static int str_stack_i = 0;
	const int str_stack_max = 16;
	
	if (!str_stack)
		str_stack = malloc_safe(str_stack_max * sizeof(*str_stack));
	
	Fnl = strchr(tsv, '\n');
	
//	fprintf(stderr, "Fnl at %ld\n", Fnl - tsv);
	
	/* count columns */
	for (cols = 0, ss = tsv; ss && ss < Fnl; ++ss, ++cols)
	{
		if (!memcmp(ss, colname, strlen(colname)))
			col = cols;
		ss = strchr(ss, '\t');
	}
	
//	fprintf(stderr, "%d columns\n", cols);
	
	/* failed to locate requested column */
	if (col < 0)
		return 0;
	
	/* nth item */
	rem = cols * row + col;
	
//	fprintf(stderr, "requested column %d\n", col);
//	fprintf(stderr, "item will be %dth item\n", rem);
	
	char *nxt = tsv; /* next starting position */
	for (cols = 0, ss = tsv; nxt; ++ss, --rem)
	{
		char *Nt;  /* next tab */
		char *Nnl; /* next newline */
		Nt = strchr(ss, '\t');
		Nnl = strchr(ss, '\n');
		
		/* if two are found, use the nearest */
		if (Nt && Nnl)
			nxt = (Nt < Nnl) ? Nt : Nnl;
		/* if one is 0, use the other */
		else
			nxt = (Nt > Nnl) ? Nt : Nnl;
		
		/* we found the item */
		if (!rem)
		{
			int len;
			
			out = str_stack[str_stack_i++].str;
			
			if (str_stack_i >= str_stack_max)
				str_stack_i = 0;
			
			if (nxt)
				len = nxt - ss;
			else
				len = strlen(ss);
			
			/* prevent overflow */
			if (len >= sizeof(str_stack[0].str))
				len = sizeof(str_stack[0].str) - 1;
			memcpy(out, ss, len);
			out[len] = '\0';
			
//			fprintf(stderr, "found it '%*s'?\n", len, out);
			
			return out;
		}
		
		ss = nxt;
	}
	
	return 0;
}

/* put 32-bit value into raw data */
void
put32(void *_data, int v)
{
	unsigned char *data = _data;
	
	data[0] = v >> 24;
	data[1] = v >> 16;
	data[2] = v >> 8;
	data[3] = v;
}

/* put 24-bit value into raw data */
void
put24(void *_data, int v)
{
	unsigned char *data = _data;
	
	data[0] = v >> 16;
	data[1] = v >> 8;
	data[2] = v;
}

/* put 16-bit value into raw data */
void
put16(void *_data, int v)
{
	unsigned char *data = _data;
	
	data[0] = v >> 8;
	data[1] = v;
}

/* put 8-bit value into raw data */
void
put8(void *_data, int v)
{
	unsigned char *data = _data;
	
	data[0] = v;
}

/* get 32-bit value from raw data */
int
get32(void *_data)
{
	unsigned char *data = _data;
	
	return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

/* get 24-bit value from raw data */
int
get24(void *_data)
{
	unsigned char *data = _data;
	
	return (data[0] << 16) | (data[1] << 8) | data[2];
}

/* get 16-bit value from raw data */
int
get16(void *_data)
{
	unsigned char *data = _data;
	
	return (data[0] << 8) | data[1];
}

/* get 32-bit value from raw data */
int
get8(void *_data)
{
	unsigned char *data = _data;
	
	return data[0];
}

/* get unsigned equivalent of signed char */
int
u8(char c)
{
	return (unsigned char) c;
}

/* get signed char equivalent of int */
int
s8(int c)
{
	return (signed char)c;
}

/* cast int to unsigned short */
int
u16(int c)
{
	return (unsigned short)c;
}

/* cast int to signed short */
int
s16(int c)
{
	return (signed short)c;
}

/* cast a and b to u32 and do operation */
unsigned int
u32op(unsigned int a, char *op, unsigned int b)
{
	a &= 0xFFFFFFFF;
	b &= 0xFFFFFFFF;
	
#define OPTEST(str, chars) if (!strcmp(op, str)) return a chars b
	OPTEST("+", +);
	else OPTEST("-", -);
	else OPTEST("*", *);
	else OPTEST("/", /);
	else OPTEST("&", &);
	else OPTEST("|", |);
	else OPTEST("<", <);
	else OPTEST(">", >);
	else OPTEST("<=", <=);
	else OPTEST(">=", >=);
	else OPTEST("==", ==);
	else OPTEST("%", %);
	else die_i("invalid u32op() op '%s'", op);
#undef OPTEST
	return 0;
}

/* get string at index `idx` in string `*list`; *
 * returns 0 if anything goes awry;             *
 * used for fetching name from name list        */
char *
substring(char *list, int index)
{
	if (!list || !*list || index < 0)
		return 0;
	
	/* advance one string forward at a time until we  *
	 * reach either the index, or the end of the list */
	while (index && *list)
	{
		list += strlen(list) + 1;
		index -= 1;
	}
	
	/* reached end of list */
	if (!*list)
		return 0;
	
	/* reached index */
	return list;
}

/* get vram size of overlay */
unsigned int
ovl_vram_sz(void *_ovl, int sz)
{
#define U32b(x) ((x)[0]<<24|(x)[1]<<16|(x)[2]<<8|(x)[3])
	unsigned char *ovl = _ovl;
	unsigned int e;
	
	if (!ovl)
		die_i("i can't ovl_vram_sz() nonexistent overlay");
	
	if (sz <= 4)
		die_i("ovl_vram_sz() sz must be > 4");
	
	e = U32b(ovl + (sz - 4));
	
	if (e < 4 || e >= sz)
		die_i("invalid overlay");
	
	ovl += sz - e;
	
	ovl += 3 * 4;
	
	/* e = ovl size */
	e = sz;
	
	/* add .bss size */
	e += U32b(ovl);
	
	return e;
#undef U32b
}

/* load png */
void *
load_png(char *fn, int *w, int *h)
{
	unsigned char *pixels;
	int c;
	
	/* if fn starts with '*', grab by extension */
	fn = find_extension(fn, 0);
	
	if (!w || !h)
		die_i("load_png() w or h is 0");
	
	/* file doesn't exist or won't load */
	if (!file_size(fn))
		return 0;
	
	pixels = stbi_load(fn, w, h, &c, STBI_rgb_alpha);
	if (!pixels)
		die_i("error loading png '%s'", fn);
	
	return pixels;
}

/****
 * directory functions
 ****/

/* returns 1 if directory of given name exists */
int
dir_exists(char *dir)
{
	struct stat stat_path;
	
	/* stat for the path */
	stat(dir, &stat_path);
	
	/* if path does not exist or is not dir, return 0 */
	if (S_ISDIR(stat_path.st_mode) == 0)
		return 0;
	
	/* is a directory */
	return 1;
}

/* enter a directory (folder)
   NOTE: directory is created if it doesn't exist */
void
dir_enter(char *dir)
{
	DEBUG_PRINT("dir_enter(%s)\n", dir);
	/* unable to enter directory */
	if (wow_chdir(dir))
	{
		/* attempt to create directory */
		if (wow_mkdir(dir))
			die_i("i could not create the directory '%s'", dir);
		
		if (wow_chdir(dir))
			die_i("i could not enter the directory '%s'", dir);
	}
}

/* enter the directory of a provided file */
void
dir_enter_file(char *fn)
{
	char *dup;
	char *ss;
	
	DEBUG_PRINT("dir_enter_file '%s'\n", fn);
	
	/* make sure this is a real string */
	if (!fn)
		die_i("you're using dir_enter_file() wrong");
	
	/* we want to operate on a copy */
	dup = strdup_safe(fn);
	
	/* strip last part of directory */
	ss = strrchr(dup, '/');
	if (!ss)
		ss = strrchr(dup, '\\');
	if (!ss)
		goto cleanup;
	else
		*ss = '\0';
	
	DEBUG_PRINT(" > means chdir('%s')\n", dup);
	
	/* now enter the directory */
	if (wow_chdir(dup))
		die_i("i can't enter the directory '%s'", dup);
	
cleanup:
	/* free the copy */
	free(dup);
}

/* leave last-entered directory */
void
dir_leave(void)
{
	if (wow_chdir(".."))
		die_i("navigation error");
}

/* make a compliant directory name */
char *
dir_mkname(int idx, char *str)
{
	char *buf;
	const int sz = 4096;
	const int s_lim = sz - 64;  /* sprintf str char limit */
	
	if (idx < 0)
		die_i("mkname idx must be >= 0, you provided %d", idx);
#if 0
	if (!str)
		die_i("mkname null string why");
#endif
	/* zero-length string */
	if (str && (isblank(*str) || iscntrl(*str)))
		str = 0;
	
	/* allocate if it doesn't already exist */
	if (!g.dir_buf)
		g.dir_buf = calloc_safe(1, sz);
	
	buf = g.dir_buf;
	
	switch (g.mkstyle)
	{
		case ZZRTL_MKSTYLE_PRE:
			if (str)
				sprintf(buf, "%d - %.*s", idx, s_lim, str);
			else
				sprintf(buf, "%d", idx);
			break;
		
		case ZZRTL_MKSTYLE_PREX:
			if (str)
				sprintf(buf, "0x%X - %.*s", idx, s_lim, str);
			else
				sprintf(buf, "0x%X", idx);
			break;
		
		case ZZRTL_MKSTYLE_POST:
			if (str)
				sprintf(buf, "%.*s (%d)", s_lim, str, idx);
			else
				sprintf(buf, "(%d)", idx);
			break;
		
		case ZZRTL_MKSTYLE_POSTX:
			if (str)
				sprintf(buf, "%.*s (0x%X)", s_lim, str, idx);
			else
				sprintf(buf, "(0x%X)", idx);
			break;
		
		default:
			die_i("invalid mkstyle");
			break;
	}
	
	return buf;
}

/* set style used by dir_mkname
   valid options:
      "pre"   : uses form "%d - %s"
      "preX"  : uses form "0x%X - %s"
      "post"  : uses form "%s (%d)"
      "postX" : uses form "%s (0x%X)"
*/
void
dir_use_style(char *style)
{
	if (!strcmp(style, "pre"))
		g.mkstyle = ZZRTL_MKSTYLE_PRE;
	else if (!strcmp(style, "preX"))
		g.mkstyle = ZZRTL_MKSTYLE_PREX;
	else if (!strcmp(style, "post"))
		g.mkstyle = ZZRTL_MKSTYLE_POST;
	else if (!strcmp(style, "postX"))
		g.mkstyle = ZZRTL_MKSTYLE_POSTX;
	else
		die_i("unknown style '%s'", style);
}

/* returns non-zero if file of given name exists, 0 otherwise */
int
file_exists(char *fn)
{
	FILE *fp;
	
	fp = fopen(fn, "rb");
	if (!fp)
		return 0;
	
	fclose(fp);
	return 1;
}

/****
 * rom functions
 ****/

/* allocate a rom structure */
struct rom *
rom_new(struct rom *rom, char *fn)
{
	struct rom *dst;
	
	/* make sure rom doesn't already exist */
#ifdef ZZRTL_NEW_TEST
	if (rom)
		die_i("i already loaded a rom, why are you loading twice");
#endif
	
	if (!fn)
		die_i("rom_new() invalid filename");
	
	/* if fn starts with '*', grab by extension */
	fn = find_extension(fn, 1);
	
	/* allocate destination rom structure */
	dst = calloc_safe(1, sizeof(*dst));
	
	/* propagate rom file */
	dst->data = file_load(fn, &dst->data_sz, 0);
	
	/* back up load file name */
	dst->fn = strdup_safe(fn);
	
	if (!dst->data)
		die_i("something went wrong loading rom '%s'", fn);
	
	/* defaults */
	dst->align = 0x10;
	dst->fdmaidx = -1;
	
	return dst;
}

/* free a rom structure */
void
rom_free(struct rom *rom)
{
	if (!rom)
		return;
	
	if (rom->data)
		free(rom->data);
	
	if (rom->dma)
		free(rom->dma);
	
	/* free any memory pools that were allocated */
	if (rom->mem.mb16)
		free(rom->mem.mb16);
	if (rom->mem.mb4)
		free(rom->mem.mb4);
	
	/* free every writable block */
	writable_free(rom->writable);
	
	free(rom);
}

/* set special flags inside a rom structure */
void
rom_flag(struct rom *rom, const char *id, int on)
{
	if (!id)
		die_i("rom_flag() no flag provided");
	if (!strcasecmp(id, "dmaext"))
		rom->flag.dmaext = on;
	else if (!strcasecmp(id, "minpad"))
		rom->flag.minpad = on;
	else
		die_i("rom_flag() '%s' invalid flag", id);
}

/* get pointer to raw data at offset `ofs` in rom */
void *
rom_raw(struct rom *rom, unsigned int ofs)
{
	/* want only 32-bit value */
	ofs &= 0xFFFFFFFF;
	
	if (!rom || !rom->data)
		die_i("i can't fetch raw data from uninitialized rom");
	
	if (ofs >= rom->data_sz)
		die_i("i can't fetch raw data beyond rom range (0x%X)", ofs);
	
	return rom->data + ofs;
}

/* save rom to disk using specified filename */
void
rom_save(struct rom *rom, char *fn)
{
#if 0
	if (!rom)
		die_i("i cannot save rom without first using rom = rom.new()");
	if (!rom->data)
		die_i("i cannot save rom without first using rom.load()");
#else
	if (!rom || !rom->data)
		die_i("i cannot save an uninitialized rom");
#endif
	
	/* updates dmadata */
	rom_write_dmadata(rom);
	
	/* recalculate crc */
	fix_crc(rom->data);
	
	if (file_write(fn, rom->data, rom->data_sz) != rom->data_sz)
		die_i("something went wrong saving rom");
}

/* set rom alignment */
void
rom_align(struct rom *rom, int align)
{
	if (!rom || !rom->data)
		die_i("i can't align() uninitialized rom");
	
	if (align & 15)
		die_i("align() requires a multiple of 16 (0x10)");
	
	if (align < 16)
		die_i("align() requires value >= 16 (0x10)");
	
	rom->align = align;
}

#include "dma__compress.h"

/* compress rom using specified algorithm
   valid options: "yaz", "lzo", "ucl"
   NOTE: to use anything besides yaz, you a patch */
void
rom_compress(struct rom *rom, char *enc, int mb)
{
	struct dma *dma;
	struct folder *list = 0;
	struct fldr_item *item;
	char *slash_enc = 0;
//	unsigned char *compbuf;
	int compbuf_sz;
	int compbuf_along = 0; /* how far along we are in compbuf */
	int (*encfunc)(
		void *src
		, unsigned int src_sz
		, void *dst
		, unsigned int *dst_sz
		, void *_ctx
	);
	void *(*ctx_new)(void) = 0;
	void (*ctx_free)(void *) = 0;
	unsigned int compsz = mb * 0x100000;
	unsigned int comp_total = 0;
	unsigned int largest_compress = 0;
	int i;
	float total_compressed = 0;
	float total_decompressed = 0;
	
	struct compThread *compThread = 0;
	int numThread = 4; /* TODO make this external */
	
	/* set preferred alignment for compressed files */
	rom->align = 0x10;
	
	if (!rom)
		die_i("cannot compress uninitialized rom");
	if (!rom->dma || !rom->dma_ready)
		die_i("cannot compress rom until dma has been initialized");
	if (rom->is_comp)
		return;
	if (compsz > rom->data_sz || mb <= 0)
		die_i("invalid rom_compress() mb");
	
	/* get encoding function */
	if (!strcmp(enc, "yaz") || !strcmp(enc, "slowyaz"))
	{
		encfunc = yazenc;
		ctx_new = yazCtx_new;
		ctx_free = yazCtx_free;
	}
	//else if (!strcmp(enc, "slowyaz")) // no application for slowyaz anymore
	//	encfunc = slowyazenc;
	else if (!strcmp(enc, "lzo"))
	{
		encfunc = lzoenc;
		ctx_new = lzoCtx_new;
		ctx_free = lzoCtx_free;
	}
	else if (!strcmp(enc, "ucl"))
	{
		encfunc = uclenc;
	}
//	else if (!strcmp(enc, "zx7"))
//	{
//		encfunc = zx7enc;
//	}
	else if (!strcmp(enc, "aplib"))
		encfunc = aplenc;
	else
		die_i("i have never heard of '%s' compression", enc);
	
/* construct new writable region table based on compressible files */
	/* free previous writable regions */
	rom->writable = writable_free(rom->writable);
	
	/* mark all compressible files as writable, to allow their
	   uncompressed data to be overwritten with compressed data */
	for (dma = rom->dma; dma - rom->dma < rom->dma_num; ++dma)
	{
		/* restore original start, end for nonexistent files */
		if (dma->deleted)
		{
			dma->start = dma->Ostart;
			dma->end   = dma->Oend;
			dma->compress = 1;
		}
		dma->writable = dma->compress;
	}
	
	/* construct new writable region table, with 0-init disabled */
	rom->dma_ready = 2;
	rom_dma_ready(rom);
	
	/* disable compression on deleted files */
	for (dma = rom->dma; dma - rom->dma < rom->dma_num; ++dma)
	{
		/* restore original start, end for nonexistent files */
		if (dma->deleted)
		{
			dma->compress = 0;
		}
	}
	
/* compress files */
#if 0 /* a different buffer is assigned to each thread instead now */
	if (!g_use_cache)
	{
		/* compression buffer needs to be large enough to fit *
		 * the compressed contents of the entire game; double *
		 * the size requested for the final compressed rom    *
		 * should be more than enough space...                */
		compbuf = malloc_safe(compsz * 2);
	}
	else
	{
		/* allocate reusable compression buffer, 16 mb */
		if (!rom->mem.mb16)
			rom->mem.mb16 = malloc_safe(SIZE_16MB);
		compbuf = rom->mem.mb16;
	}
#endif
	
	/* sort dma entries by size, descending */
	qsort(
		rom->dma
		, rom->dma_num
		, sizeof(*rom->dma)
		, dma_sort_size_descending
	);
	
	/* locate largest file that will be compressed */
	for (dma = rom->dma; dma - rom->dma < rom->dma_num; ++dma)
	{
		if (dma->compress && dma->end - dma->start > largest_compress)
			largest_compress = dma->end - dma->start;
	}
	
	/* no file should compress to be 2x its non-compressed size */
	largest_compress *= 2;
	
	/* allocate compression buffer for each thread */
	compThread = calloc_safe(numThread, sizeof(*compThread));
	for (i = 0; i < numThread; ++i)
	{
		compThread[i].data = malloc_safe(largest_compress);
	
		/* allocate compression contexts (if applicable) */
		if (ctx_new)
		{
			compThread[i].ctx = ctx_new();
			if (!compThread[i].ctx)
				die_i("memory error");
		}
	}
	
	/* if using compression cache */
	if (g_use_cache)
	{
		/* create and enter cache folder */
		dir_enter("cache");
		
		/* create and enter directory for the encoding algorithm */
		dir_enter(enc);
		
		/* transform "yaz" to "/yaz" */
		/* +1 for additional '/', and +1 for 0-term */
		slash_enc = malloc_safe(strlen(enc) + 2);
		strcpy(slash_enc, "/");
		strcat(slash_enc, enc);
		
		/* get list of files with extension enc (ex "abc.yaz") */
		/* was slash_enc before */
		/* changed to "//x" so it gets ALL extensions (.yaz and .raw) */
		list = folder_new(list, "//x");
		
		/* change "/yaz" to ".yaz" */
		slash_enc[0] = '.';
	}
	
	/* now compress every compressible file */
#ifdef NOTHREADS
	dma__compress(
		rom
		, compThread[0].data
		, encfunc
		, enc
		, slash_enc
		, list
		, 1          /* stride */
		, 0          /* ofs    */
		, 1          /* report */
		, compThread[0].ctx
	);
#else
	/* spawn threads */
	for (i = 0; i < numThread; ++i)
	{
		dma__compress_thread(
			&compThread[i]
			, rom
			, compThread[i].data
			, encfunc
			, enc
			, slash_enc
			, list
			, numThread  /* stride */
			, i          /* ofs    */
			, (i+1)==numThread      /* report */
			, compThread[i].ctx
		);
	}

	/* wait for all threads to complete */
	for (i = 0; i < numThread; ++i)
	{
		if (pthread_join(compThread[i].pt, NULL))
			die_i("threading error");
	}
#endif
	int progress_a = min_int(dma - rom->dma, rom->injected);
	int progress_b = rom->injected;
	if (!rom->injected)
	{
		progress_a = dma - rom->dma;
		progress_b = rom->dma_num - 1;
	}
	display_compression_progress(
		enc
		//, rom->dma_num-1
		//, rom->dma_num-1
		, progress_a
		, progress_b
	);
	fprintf(stderr, "success!\n");
	
	/* zero-initialize the writable regions */
	if (rom->writable)
		rom_writable_zero(rom);
	
//	fprintf(stderr, "%08X bytes of compressed data\n", comp_total);
	
#if 0
	/* cap writable regions to compressed rom size */
	if (rom->writable)
	{
		struct writable *wr;
	
		for (wr = rom->writable; wr; wr = wr->next)
		{
			/* lies outside compressed rom space, so discard */
			if (wr->start > compsz)
				wr->start = wr->len = 0;
			
			/* start is in compressed space, so trim length */
			else if (wr->start + wr->len > compsz)
				wr->len = compsz - wr->start;
		}
	}
#endif
	
	/* sort by original start, ascending */
	qsort(
		rom->dma
		, rom->dma_num
		, sizeof(*rom->dma)
		, dma_sort_start_ascending
	);
	
	/* zero the entire rom */
	memset(rom->data, 0, compsz);
	
	/* go through dma table, injecting compressed files */
	comp_total = 0;
	for (dma = rom->dma; dma - rom->dma < rom->dma_num; ++dma)
	{
		struct writable *wr;
		unsigned char *dst;
		char *fn = dma->compname;
		unsigned int sz;
		unsigned int sz4;
		
		int progress_a = min_int(dma - rom->dma, rom->injected);
		int progress_b = rom->injected;
		if (!rom->injected)
		{
			progress_a = dma - rom->dma;
			progress_b = rom->dma_num - 1;
		}
		fprintf(
			stderr
			, "\r""injecting file %d/%d: "
			, progress_a
			, progress_b
		);
		
#if 0
		fprintf(
			stderr
			, "%08X %08X (%d)\n"
			, dma->start
			, dma->end
			, dma->compress
		);
#endif
		
		/* cached file logic */
		if (g_use_cache)
		{
			/* skip entries that don't reference compressed files */
			if (!fn)
				continue;
			
			sz = dma->compSz;
			
			/* sz == 0 */
			if (!sz)
				die_i("'cache/%s/%s' sz == 0", enc, fn);
		}
		
		/* in-memory file logic */
		else
		{
			/* skip entries that don't reference compressed data */
			sz = dma->compSz;
			if (!sz)
				continue;
		}
		
#if 0 /* FIXME actually, don't; this corrupts files;
         injecting files that are larger when compressed
         is still beneficial for the sake of contiguous blocks */
		/* skip entries that don't benefit from compression */
		/* FIXME this is now taken care of elsewhere *
		 *       and it works like a dream!          */
		if (sz >= (dma->end - dma->start))
			continue;
#endif
//		if (sz >= (dma->end - dma->start))
//			fprintf(stderr, "warning: compSz larger by %08X\n", sz - (dma->end - dma->start));
		
		/* ensure we remain 16-byte-aligned after advancing */
		sz4 = sz;
		if (sz4 & 15)
			sz4 += 16 - (sz4 & 15);
		
		/* put the files in */
		dst = rom->data + comp_total;
		
		/* r4: don't forget file prefixes */
		if (dma->has_prefix)
		{
			memcpy(dst, dma->prefix, dma->has_prefix);
			dst += dma->has_prefix;
			comp_total += dma->has_prefix;
		}
		
		dma->Pstart = comp_total;
		if (dma->compress)
		{
			dma->Pend = dma->Pstart + sz4;
			
			/* compressed file ratio variables */
			total_compressed += sz4;
			total_decompressed += dma->end - dma->start;
		}
		else
			dma->Pend = 0;
		comp_total += sz4;
		
#if 0
		/* get region where this file will fit */
//		fprintf(stderr, "need something to fit %08X\n", sz4);
		wr = rom_get_region(rom, sz4);
		
		/* get rom pointer */
		dst = rom->data + wr->start;
		
		/* compressed location = physical address */
		dma->Pstart = wr->start;
		dma->Pend = wr->start + sz4;
		
		/* update writable region */
		wr->start += sz4;
		wr->len -= sz4;
#endif
//		fprintf(stdout, "%08X: %08X %08X\n", dma - rom->dma, dma->Pstart, dma->Pend);
//		fprintf(stdout, "%08X: %08X %08X %08X\n", dma - rom->dma, dma->start, dma->Pstart, dma->Pend);
		if (dma->Pend > compsz)
			die_i("i ran out of compressed rom space");
		
		/* external cached file logic */
		if (g_use_cache)
		{
			/* load file into rom at offset */
			dst = file_load_into(fn, &sz, dst);
			if (!dst)
				die_i("'cache/%s/%s' read error", enc, fn);
		}
		
		/* otherwise, a simple memcpy */
		else
		{
			memcpy(dst, dma->compbuf, dma->compSz);
		}
	}
	fprintf(stderr, "success!\n");
	
	fprintf(
		stderr
		, "compression ratio: %.02f%%\n"
		, (total_compressed/total_decompressed) * 100
	);
	
	/* now free compressed file names */
	for (dma = rom->dma; dma - rom->dma < rom->dma_num; ++dma)
		if (dma->compname)
			free(dma->compname);
	
	/* remove unused cache files */
	if (list)
	{
		for (item = list->item; item - list->item < list->count; ++item)
		{
			/* udata hasn't been marked, so file is unused */
			if (item->name && !item->udata)
				if (remove(item->name))
					die_i("failed to remove 'cache/%s/%s'", enc, item->name);
		}
	}
	
	/* navigate back to parent directory */
	if (g_use_cache)
	{
		dir_leave(); /* exit enc   */
		dir_leave(); /* exit cache */
	}
	
	/* update rom size for when rom_save() is used */
	rom->data_sz = compsz;
	
	/* zero starts/ends of deleted files */
	for (dma = rom->dma; dma - rom->dma < rom->dma_num; ++dma)
	{
		/* restore original start, end for nonexistent files */
		if (dma->deleted)
		{
			dma->start = 0;
			dma->end = 0;
			dma->Pstart = 0;
			dma->Pstart = 0;
		}
		
		/* free any compbufs */
		if (dma->compbuf)
			free(dma->compbuf);
		
		/* not necessary, but a good practice just in *
		 * case this data is reused in the future...  */
		dma->compSz = 0;
		dma->compbuf = 0;
	}
	
	/* cleanup */
	if (list)
		folder_free(list);
	if (slash_enc)
		free(slash_enc);
#if 0
	if (!g_use_cache)  /* compbuf was locally allocated */
		free(compbuf);
#else
	for (i = 0; i < numThread; ++i)
	{
		free(compThread[i].data);
	
		/* free compression contexts (if applicable) */
		if (ctx_free)
		{
			assert(compThread[i].ctx);
			ctx_free(compThread[i].ctx);
		}
	}
	free(compThread);
#endif
}

/* seek offset within rom */
void
rom_seek(struct rom *rom, unsigned int offset)
{
	if (!rom || !rom->data)
		die_i("i cannot seek inside uninitialized rom");
	
	rom->ofs = offset;
	
	if (rom->ofs >= rom->data_sz)
		die_i("seek()'d too far forward (0x%X)", offset);
}

/* seek offset within rom, relative to current offset */
void
rom_seek_cur(struct rom *rom, int offset)
{
	if (!rom || !rom->data)
		die_i("i cannot seek inside uninitialized rom");
	
	/* cast, guaranteeing forward and backwards seeking */
	/* TODO confirm backwards seeking works */
	offset = (int32_t)offset;
	
	if (offset < 0 && abs_int(offset) > rom->ofs)
		die_i("seeking %d is too far back", offset);
	
	rom_seek(rom, rom->ofs + offset);
}

/* get offset within rom */
unsigned int
rom_tell(struct rom *rom)
{
	if (!rom || !rom->data)
		die_i("i cannot tell() inside uninitialized rom");
	
	return rom->ofs;
}

/* get size of rom */
unsigned int
rom_size(struct rom *rom)
{
	if (!rom || !rom->data)
		die_i("i cannot size() uninitialized rom");
	
	return rom->data_sz;
}

/* inject file into rom */
void *
rom_inject(struct rom *rom, char *fn, int comp)
{
	unsigned char *rval;
	
	if (!rom || !rom->data)
		die_i("i cannot inject into uninitialized rom");
	
	if (!fn)
		die_i("rom_inject() no filename provided");
	
	/* inject file into rom, noting the offset within the rom */
	rval = rom_inject_file(rom, fn, comp);
	rom->injected += 1;
	
	/* return resulting injection offset */
	return rval;
}

#if 0
void
rom_file_header(struct rom *rom, unsigned char *header/*16*/)
{
	/* TODO */
	if (!rom || !header)
		die("rom.file_header() invalid argument(s)");
	
	struct dma *dma = rom->dma_last;
	if (!dma)
		die("rom.file_header() no file injected");
	memcpy(dma->header, header, 16);
	dma->has_header = 1;
}
#endif

/* enable injection prefix */
void *
rom_inject_prefix(struct rom *rom)
{
	if (!rom)
		die("rom.inject_prefix() invalid argument(s)");
	
	memset(rom->inject_prefix, 0, 16);
	
	//if (prefix)
	//	memcpy(rom->inject_prefix, prefix, 16);
	rom->has_injection_prefix = 16;
	return rom->inject_prefix;
}

/* use special injection mode */
void
rom_inject_mode(struct rom *rom, char *mode)
{
	if (!rom)
		die("rom.inject_mode() invalid argument(s)");
	
	/* use defaults */
	if (!mode)
	{
		rom->inject_mode = INJECTMODE_DEFAULT;
		return;
	}
	
	if (!strcasecmp(mode, "earliest"))
		rom->inject_mode = INJECTMODE_EARLIEST;
	
	else if (!strcasecmp(mode, "latest"))
		rom->inject_mode = INJECTMODE_LATEST;
	
	else if (
		!strcasecmp(mode, "smallest")
		|| !strcasecmp(mode, "default")
		|| !strcasecmp(mode, "0")
	)
		rom->inject_mode = INJECTMODE_DEFAULT;
	
	else if (!strcasecmp(mode, "largest"))
		rom->inject_mode = INJECTMODE_LARGEST;
	
	else
		die("rom.inject_mode() unknown mode '%s'", mode);
}

/* get start offset of file injected with inject() */
/* NOTE: will be 0 if inject() failed */
unsigned int
rom_file_start(struct rom *rom)
{
	if (!rom || !rom->data)
		die_i("i cannot file_start() in uninitialized rom");
	
	return rom->fstart;
}

/* get end offset of file injected with inject() */
/* NOTE: will be 0 if inject() failed */
unsigned int
rom_file_end(struct rom *rom)
{
	if (!rom || !rom->data)
		die_i("i cannot file_end() in uninitialized rom");
	
	return rom->fend;
}

/* get pointer to data of most recently written
   file; will be 0 if inject() failed */
void *
rom_file(struct rom *rom)
{
	if (!rom || !rom->data)
		die_i("i cannot file() in uninitialized rom");
	
	return rom->file;
}

/* get size of file injected with inject() */
/* NOTE: will be 0 if inject() failed */
unsigned int
rom_file_sz(struct rom *rom)
{
	if (!rom || !rom->data)
		die_i("i cannot file_sz() in uninitialized rom");
	
	return rom->fend - rom->fstart;
}

/* get dma index of most recently written file (-1 if none) */
int
rom_file_dma(struct rom *rom)
{
	if (!rom || !rom->data)
		die_i("i cannot file_dma() in uninitialized rom");
	
	/* r5: disable reorder on files that file_dma() is used on */
	if (rom->dma_last)
	{
		/* +1 to account for 0 indicating no forced order */
		rom->dma_last->forceIndex = rom->fdmaidx + 1;
	}
	
	return rom->fdmaidx;
}

/* extract raw data to a file */
void
rom_extract(struct rom *rom, char *fn, unsigned start, unsigned end)
{
	DEBUG_PRINT("rom_extract('%s', 0x%X, 0x%X)\n", fn, start, end);
	if (start == end)
		return;
	if (!rom || !rom->data)
		die_i("i cannot extract from uninitialized rom");
	if (end < start)
		die_i(
			"i cannot extract file when end (0x%X) < start (0x%X)"
			, end
			, start
		);
	if (end > rom->data_sz)
		die_i("end offset 0x%X exceeds rom size", end);
	
	unsigned sz = end - start;
	
	if (file_write(fn, rom->data + start, sz) != sz)
		die_i(
			"something went wrong with extracting file (%08X - %08X)"
			, start
			, end
		);
}

/* extract raw data to a file, based on dma entry */
void
rom_extract_dma(struct rom *rom, char *fn, int idx)
{
	struct dma *dma;
	unsigned int start, end;
	
	DEBUG_PRINT("rom_extract('%s', 0x%X, 0x%X)\n", fn, start, end);
	if (!rom || !rom->data)
		die_i("i cannot extract from uninitialized rom");
	if (!rom->dma || !rom->dma_num)
		die_i("rom.extract_dma cannot extract: uninitialized dma");
	
	if (idx >= rom->dma_num || idx < 0)
		die_i(
			"rom.extract_dma(%d) exceeds dma table (%d)\n"
			, idx
			, rom->dma_num
		);
	
	dma = rom->dma + idx;
	end = dma->Oend;
	start = dma->Ostart;
	
	if (end < start)
		die_i(
			"rom.extract_dma(%d): "
			"i cannot extract file when end (0x%X) < start (0x%X)"
			, idx
			, end
			, start
		);
	if (end > rom->data_sz)
		die_i(
			"rom.extract_dma(%d): "
			"end offset 0x%X exceeds rom size"
			, idx
			, end
		);
	
	unsigned sz = end - start;
	
	if (file_write(fn, rom->data + start, sz) != sz)
		die_i(
			"rom.extract_dma(%d): "
			"something went wrong with extracting file (%08X - %08X)"
			, idx
			, start
			, end
		);
}

/* loads PNG, converts to N64 format, and injects into rom */
void *
rom_inject_png(
	struct rom *rom
	, char *fn
	, enum n64texconv_fmt fmt
	, enum n64texconv_bpp bpp
	, int comp
)
{
	const char *err;
	unsigned char *pixels;
	unsigned int sz;
	int w;
	int h;
	
	pixels = load_png(fn, &w, &h);
	
	if (!pixels)
	{
		rom->fstart = 0;
		rom->fend = 0;
		rom->file = 0;
		return 0;
	}
	
	err =
	n64texconv_to_n64(
		pixels
		, pixels
		, 0 /* pal */
		, 0 /* pal_colors */
		, fmt
		, bpp
		, w
		, h
		, &sz
	);
	
	if (err)
		die("[!] texture conversion error: '%s'", err);
	
	//fprintf(stderr, "resulting sz = 0x%X\n", sz);
	//print_sha1(pixels, w);
	
	/* inject into rom */
	rom_inject_raw(rom, pixels, sz, comp);
	rom->injected++;
	
	/* test conversion */
//	rom_extract_png(rom, "out.png", 0, rom->fstart, 0, w, h, fmt, bpp);
	
	//print_sha1(rom->file, w);
	
	free(pixels);
	
	return rom->file;
}

/* converts raw texture data to rgba8888, saves as PNG */
void
rom_extract_png(
	struct rom *rom
	, char *fn
	, void *buf
	, int texofs
	, int palofs
	, int w
	, int h
	, enum n64texconv_fmt fmt
	, enum n64texconv_bpp bpp
)
{
	unsigned char *dst;
	unsigned char *tex;
	unsigned char *pal;
	const char *err;
	
	if (!rom || !rom->data)
		die_i("i can't extract from an uninitialized rom");
	if (!fn)
		die_i("you didn't provide an extract_png() filename");
	if (texofs >= rom->data_sz)
		die_i("extract_png() texofs 0x%X out of range", texofs);
	if (palofs >= rom->data_sz)
		die_i("extract_png() palofs 0x%X out of range", palofs);
	
	/* user provided destination buffer; use it */
	if (buf)
		dst = buf;
	
	/* user didn't provide destination buffer; allocate one */
	else
		dst = malloc_safe(w * h * 4);
	
	tex = rom->data + texofs;
	pal = rom->data + palofs;
	
	err = n64texconv_to_rgba8888(dst, tex, pal, fmt, bpp, w, h);
	
	if (err)
		die("[!] texture conversion error: '%s'", err);
	
	stbi_write_png(fn, w, h, 4, dst, 0);
	
	/* free the destination buffer we alloc'd */
	if (!buf)
		free(dst);
}

/* write and advance `num` bytes within rom */
void
rom_write(struct rom *rom, void *data, int sz)
{
	unsigned char *raw;
	
	if (!rom || !rom->data)
		die_i("i cannot write into uninitialized rom");
	if (!data || !sz)
		die_i("i cannot write empty data into rom");
	if (rom->ofs + sz > rom->data_sz)
		die_i(
			"can't write %d bytes at 0x%X b/c it exceeds rom size",
			rom->ofs
		);
	
	raw = rom->data + rom->ofs;
	
	memcpy(raw, data, sz);
	
	rom->ofs += sz;
}

/* write and advance 4 bytes within rom */
void
rom_write32(struct rom *rom, unsigned int value)
{
	unsigned char raw[4];
	
	raw[0] = value >> 24;
	raw[1] = value >> 16;
	raw[2] = value >>  8;
	raw[3] = value;
	
	rom_write(rom, raw, 4);
}

/* write and advance 3 bytes within rom */
void
rom_write24(struct rom *rom, int value)
{
	unsigned char raw[3];
	
	raw[0] = value >> 16;
	raw[1] = value >>  8;
	raw[2] = value;
	
	rom_write(rom, raw, 3);
}

/* write and advance 2 bytes within rom */
void
rom_write16(struct rom *rom, int value)
{
	unsigned char raw[2];
	
	raw[0] = value >>  8;
	raw[1] = value;
	
	rom_write(rom, raw, 2);
}

/* write and advance 1 byte within rom */
void
rom_write8(struct rom *rom, int value)
{
	unsigned char raw[1];
	
	raw[0] = value;
	
	rom_write(rom, raw, 1);
}

/* read and advance 4 bytes within rom */
int
rom_read32(struct rom *rom)
{
	unsigned char *raw;
	int value;
	
	if (!rom || !rom->data)
		die_i("i cannot read from uninitialized rom");
	
	if (rom->ofs > rom->data_sz || rom->ofs + 4 > rom->data_sz)
		die_i("reading at 0x%X would exceed rom range", rom->ofs);
	
	raw = rom->data + rom->ofs;
	
	value = get32(raw);
	
	rom->ofs += 4;
	
	return value;
}

/* read and advance 3 bytes within rom */
int
rom_read24(struct rom *rom)
{
	unsigned char *raw;
	int value;
	
	if (!rom || !rom->data)
		die_i("i cannot read from uninitialized rom");
	
	if (rom->ofs > rom->data_sz || rom->ofs + 3 > rom->data_sz)
		die_i("reading at 0x%X would exceed rom range", rom->ofs);
	
	raw = rom->data + rom->ofs;
	
	value = get24(raw);
	
	rom->ofs += 3;
	
	return value;
}

/* read and advance 2 bytes within rom */
int
rom_read16(struct rom *rom)
{
	unsigned char *raw;
	int value;
	
	if (!rom || !rom->data)
		die_i("i cannot read from uninitialized rom");
	
	if (rom->ofs > rom->data_sz || rom->ofs + 2 > rom->data_sz)
		die_i("reading at 0x%X would exceed rom range", rom->ofs);
	
	raw = rom->data + rom->ofs;
	
	value = get16(raw);
	
	rom->ofs += 2;
	
	return value;
}

/* read and advance 1 byte within rom */
int
rom_read8(struct rom *rom)
{
	unsigned char *raw;
	int value;
	
	if (!rom || !rom->data)
		die_i("i cannot read from uninitialized rom");
	
	if (rom->ofs > rom->data_sz || rom->ofs + 1 > rom->data_sz)
		die_i("reading at 0x%X would exceed rom range", rom->ofs);
	
	raw = rom->data + rom->ofs;
	
	value = get8(raw);
	
	rom->ofs += 1;
	
	return value;
}

/* get filename of rom */
char *
rom_filename(struct rom *rom)
{
	if (!rom)
		return 0;
	
	return rom->fn;
}

static
inline
void
dma_decompress(struct dma *dma, int dma_num, unsigned char *dec, unsigned char *comp)
{
	/* transfer files from comp to dec */
	for ( ; dma_num--; ++dma)
	{
		/* an entry that is used */
		if (dma->end > dma->start)
		{
			/* not compressed */
			if (!dma->Pend)
				memcpy(dec + dma->start, comp + dma->Pstart, dma->end - dma->start);
			
			/* compressed */
			else
				yazdec(
					comp + dma->Pstart       /* src */
					, dec + dma->start       /* dst */
					, dma->end - dma->start  /* dstSz */
					, 0                      /* srcSz */
				);
			
			dma->Pstart = dma->start;
			dma->Pend = 0;
		}
	}
}

/* register offset in rom, at which to write dma index (u16) */
void
rom_dma_register(struct rom *rom, unsigned int offset)
{
	if (!rom)
		die_i("i cannot dma_register() an uninitialized rom");
	
	if (offset >= rom->data_sz)
		die_i("dma_register() invalid offset %08X", offset);
	
	if (!rom->dma_last)
		die_i("dma_register(%08X): with no file injected", offset);
	
	struct dma *dma = rom->dma_last;
	struct dmareg *reg = malloc_safe(sizeof(*reg));
	reg->next = dma->reglist;
	reg->offset = offset;
	dma->reglist = reg;
}

/* grab injection prefix */
void *
rom_dma_prefix(struct rom *rom)
{
	if (!rom)
		die_i("i can't dma_prefix() an uninitialized rom");
	
	if (!rom->dma_last)
		die_i("dma_prefix() called with no file injected");
	
	if (!rom->dma_last->has_prefix)
		die_i(
			"dma_prefix() called without using inject_prefix() "
			"prior to injection"
		);
	
	return rom->dma_last->prefix;
}

/* get offset of file by dma index */
unsigned int
rom_dma_offset(struct rom *rom, int idx)
{
	return dma_get_idx(rom, idx)->start;
}

/* specify start of dmadata, and number of entries */
void
rom_dma(struct rom *rom, unsigned int offset, int num_entries)
{
	struct dma *dma;
	unsigned char *raw;
	unsigned int is_compressed = 0;
	
	if (!rom || !rom->data)
		die_i("i cannot dma() an uninitialized rom");
	
	if (num_entries <= 0)
		die_i("how am i supposed to initialize %d entries", num_entries);
	
	if (rom->dma)
		die_i("you have called rom_dma() more than once");
	
	dma = calloc_safe(num_entries, sizeof(*dma) * 3);
	rom->dma = dma;
	rom->dma_num = num_entries;
	rom->dma_sz = num_entries * 16;
	
	/* up to twice as many dmadata entries can fit in same space */
	if (rom->flag.dmaext)
		rom->dma_num *= 2;
	
	
	raw = rom->data + offset;
	rom->dma_raw = raw;
	
	/* 1060 magic */
	const unsigned char rawData[20] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x60, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x60
	};
	if (memcmp(raw, rawData, sizeof(rawData)))
	{
		die_i("incompatible rtl/rom combination; either change roms, or edit"
			" the rtl file to select the proper version of your rom");
	}
	
	/* initialize every entry */
	while (dma - rom->dma < num_entries)
	{
		/* propagate defaults */
		dma->index  = dma - rom->dma;
		dma->start  = get32(raw);
		dma->end    = get32(raw + 4);
		dma->Pstart = get32(raw + 8);
		dma->Pend   = get32(raw + 12);
		dma->Ostart = dma->start;
		dma->Oend   = dma->end;
		//dma->compress = 1; // compression should be OFF by default
		
		/* nonexistent file */
		if (dma->Pstart == dma->Pend && dma->Pstart == -1)
		{
			dma->deleted = 1;
			dma->start = 0;
			dma->end = 0;
			dma->Ostart = 0;
			dma->Oend = 0;
			dma->Pstart = 0;
			dma->Pend = 0;/*r3*/
			goto advance;
		}
		
		/* fatal error condition */
		if (
			(dma->Pend & 15)
			|| (dma->Pstart & 15)
			|| (dma->start  & 15)
			|| (dma->end & 15)
			|| dma->start > dma->end
			|| (dma->Pstart > dma->Pend && dma->Pend)
			|| dma->Pend > rom->data_sz
		)
		{
			die_i(
				"encountered dma entry %08X %08X %08X %08X; "
				"i don't know what to do"
				, dma->start, dma->end, dma->Pstart, dma->Pend
			);
		}
		
		/* rom is compressed */
		if (dma->Pend)
		{
			/*die_i(
				"encountered dma entry %08X %08X %08X %08X; "
				"is this rom compressed? "
				"i can't work with compressed roms"
				, dma->start, dma->end, dma->Pstart, dma->Pend
			);*/
			/* as of r3, compressed roms are supported */
			if (dma->end > is_compressed)
				is_compressed = dma->end;
		}
		
advance:
		/* advance to next entry */
		raw += 16;
		dma += 1;
	}
	
	/* we decompress the rom if it is compressed */
	if (is_compressed)
	{
		while (rom->data_sz < is_compressed)
			rom->data_sz *= 2;
		
		unsigned char *decompressed = calloc_safe(1, rom->data_sz);
		dma_decompress(rom->dma, num_entries, decompressed, rom->data);
		free(rom->data);
		rom->data = decompressed;
		rom->dma_raw = rom->data + offset;
		
		/* update dmadata */
		unsigned int oofs = rom->ofs;
		
		/* write every entry */
		for (dma = rom->dma; dma - rom->dma < num_entries; ++dma)
		{
			/* skip deleted files */
			if (dma->deleted)
				continue;
			
			/* go to original index location */
			rom->ofs = offset + dma->index * 16;
			
			/* write new dmadata entry */
			rom_write32(rom, dma->start);
			rom_write32(rom, dma->end);
			rom_write32(rom, dma->Pstart);
			rom_write32(rom, dma->Pend);
		}
		rom->ofs = oofs;
	
		
		/* save decompressed rom copy */
		char *Oext = is_ext(rom->fn, "z64");
		char *newfn = malloc_safe(strlen(rom->fn) + 16);
		if (Oext)
		{
			Oext -= 1; /* go back to period */
			strcpy(newfn, rom->fn);
			newfn[Oext - rom->fn] = '\0';
		}
		else
			strcpy(newfn, rom->fn);
		strcat(newfn, "-dec.z64");
		free(rom->fn);
		rom->fn = newfn;
		rom->dma_raw = 0; /* turns off writing sorted dmadata */
		rom_save(rom, newfn);
		rom->dma_raw = rom->data + offset;
	}
}

/* call this function when you're finished with the dma stuff;
   must be called before you inject data */
void
rom_dma_ready(struct rom *rom)
{
	struct dma *dma;
	int count;
	unsigned int lowest = 0;
	struct
	{
		int  active;          /* constructing contiguous block */
		unsigned int start;  /* start offset */
		unsigned int end;    /* end offset */
	} contig = {0};
	unsigned int highest_end = 0; /* highest end dma offset */
	
	if (!rom || !rom->data || !rom->dma)
		die_i("i cannot dma_ready() an uninitialized rom");
	
	/* dma_ready() has already been called */
	if (rom->dma_ready & 1)
		return;
	
	/* aliases */
	dma = rom->dma;
	count = rom->dma_num;
	
/* derive writable regions from dma table */
	
	/* sort by start offset, ascending */
	qsort(
		dma
		, count
		, sizeof(*dma)
		, dma_sort_start_ascending
	);
	
	/* confirm no entries overlap */
	for (dma = rom->dma ; dma - rom->dma < count; ++dma)
	{
		/* nonexistent file FFFFFFFF = free space */
		if (dma->Pstart == dma->Pend && dma->Pstart == -1)
		{
			dma->deleted = 1;
			dma->Ostart = 0;
			dma->Oend = 0;
			dma->start = 0;    /* r3 fix: start = end = 0, b/c data  */
			dma->end = 0;      /* pointed to is still used elsewhere */
			dma->writable = 1; /* mark for overwriting */
			dma->compress = 1; /* mark for overwriting during compress */
			continue; /* r3 fix */
		}
		
		/* skip blank entries */
		if (!dma->start && !dma->end)
			continue;
		
		/* fatal error entries where end <= start */
		if (dma->end <= dma->start)
			die_i(
				"dma invalid entry %d (%08X <= %08X)"
				, dma->index, dma->end, dma->start
			);
		
		/* fatal error on unaligned entries */
		if ((dma->start & 3) || (dma->end & 3))
			die_i(
				"dma unaligned pointer (%08X %08X)"
				, dma->start
				, dma->end
			);
		
		/* fatal error on entries exceeding rom size */
		if (dma->end > rom->data_sz)
			die_i(
				"dma entry %d (%08X - %08X) exceeds rom size (%08X)"
				, dma->index, dma->start, dma->end, rom->data_sz
			);
		
		/* if at least one entry has been processed, and its
		   start is lower than any of the previous ends */
		if (dma > rom->dma && dma->start < lowest)
			die_i(
				"dma table entry %d (%08X - %08X) "
				"overlaps entry %d (%08X - %08X)"
				, dma->index, dma->start, dma->end
				, (dma-1)->index, (dma-1)->start, (dma-1)->end
			);
		
		/* store highest dma end offset */
		if (dma->end > highest_end)
			highest_end = dma->end;
		
		/* contiguous block construction */
		if (dma->writable)
		{
			/* first writable block found; we start it at
			   the end of the entry preceding this one */
			if (!contig.active)
			{
				contig.active = 1;
				contig.start = lowest;
			}
		}
		else if (contig.active)
		{
			/* contig end is going to be the start of this
			   read-only entry we found */
			contig.end = dma->start;
			
			/* now register this writable block */
			rom_writable(rom, contig.start, contig.end);
			
			/* end of current contiguous block */
			contig.active = 0;
		}
		
		/* lowest acceptable start for next entry is end of current */
		lowest = dma->end;
		
		/* mark all writable entries as unused */
		if (dma->writable && !(rom->dma_ready & 2))
		{
			dma->start = 0;
			dma->end = 0;
			dma->Pstart = 0;
			dma->Pend = 0;
		}
	}
	
	/* if no longer constructing contiguous block, use
	   highest end dma offset for the start of remainder of rom */
	if (!contig.active)
	{
		contig.start = highest_end;
	}
	
	/* remainder of rom */
	contig.end = rom->data_sz;
	
	/* now register this writable block */
	if (contig.start < contig.end)
		rom_writable(rom, contig.start, contig.end);
	
	display_writable_regions(rom);
	
/* possibly enable this in release */
#if 0
	/* make sure we actually derived writable blocks from this */
	if (!rom->writable)
		die_i("no writable blocks derived from dma table");
#endif
	
	/* zero-initialize writable regions */
	if (rom->writable && !(rom->dma_ready & 2))
		rom_writable_zero(rom);
	
	/* note dma_ready() has been called */
	rom->dma_ready = 1;
}

/* apply dma_queue_one() to indices from start to
   end, inclusive (aka start <= idx <= end) */
void
rom_dma_queue(struct rom *rom, int start, int end)
{
	int comp = 0;
	int comp_only = 0;//comp & NWRITABLE;
	char *fname = (comp_only) ? "compress" : "queue";
	
	if (!rom || !rom->data || !rom->dma)
		die_i("i can't dma_%s an uninitialized rom", fname);
	
	if (rom->dma_ready)
		die_i("i can't dma_%s after dma_ready has been called", fname);
	
	/* swap start and end if they are not in ascending order */
	if (end < start)
	{
		int t = end;
		end = start;
		start = t;
	}
	
	/* we can't accept negative values */
	if (start < 0)
		die_i("how am i supposed to dma_%s index %d", fname, start);
	
	/* this is inclusive, aka, start <= idx <= end */
	while (start <= end && start < rom->dma_num)
	{
		struct dma *dma = rom->dma + start;
		
		/* update `writable` if NWRITABLE flag wasn't passed */
		if (!comp_only)
			dma->writable = 1;
		
		/* compression is based on lowest bit */
		dma->compress = comp & 1;
		
		start += 1;
	}
}

/* mark one dma entry (by index) as overwritable */
void
rom_dma_queue_one(struct rom *rom, int idx)
{
	rom_dma_queue(rom, idx, idx);
}

/* set compression flag on one dma entry (by index);
   if (comp == 1), file is marked for compression */
void
rom_dma_compress_one(struct rom *rom, int idx, int comp)
{
	rom_dma_compress(rom, idx, idx, comp | NWRITABLE);
}

/* apply rom_dma_compress() to indices from start to
   end, inclusive (aka start <= idx <= end) */
void
rom_dma_compress
(struct rom *rom, int start, int end, int comp)
{
	int comp_only = 1;//comp & NWRITABLE;
	char *fname = (comp_only) ? "compress" : "queue";
	
	if (!rom || !rom->data || !rom->dma)
		die_i("i can't dma_%s an uninitialized rom", fname);
	
	if (rom->dma_ready)
		die_i("i can't dma_%s after dma_ready has been called", fname);
	
	/* swap start and end if they are not in ascending order */
	if (end < start)
	{
		int t = end;
		end = start;
		start = t;
	}
	
	/* we can't accept negative values */
	if (start < 0)
		die_i("how am i supposed to dma_%s index %d", fname, start);
	
	/* this is inclusive, aka, start <= idx <= end */
	while (start <= end && start < rom->dma_num)
	{
		struct dma *dma = rom->dma + start;
		
		/* update `writable` if NWRITABLE flag wasn't passed */
		if (!comp_only)
			dma->writable = 1;
		
		/* compression is based on lowest bit */
		dma->compress = comp & 1;
		
		start += 1;
	}
}

/* reencode one existing archive within rom, by index */
void
rom_dma_rearchive_one(
	struct rom *rom
	, int idx
	, char *from
	, char *to
	, int relocate
)
{
	rom_dma_rearchive(rom, idx, idx, from, to, relocate);
}

/* reencode existing archives within rom
   NOTE: must be used before dma_ready() */
void
rom_dma_rearchive(
	struct rom *rom
	, int start
	, int end
	, char *from
	, char *to
	, int relocate
)
{
	/* TODO possible threading */
	char *fname = "rearchive";
	int (*encfunc)(
		void *src
		, unsigned int src_sz
		, void *dst
		, unsigned int *dst_sz
		, void *_ctx
	);
	void *(*ctx_new)(void) = 0;
	void (*ctx_free)(void *) = 0;
	int (*decfunc)(
		void *src, void *dst, unsigned int dstSz, unsigned int *srcSz
	);
	void *ctx = 0;
	
	if (!rom || !rom->data || !rom->dma)
		die_i("i can't dma_%s an uninitialized rom", fname);
	
// XXX we can now: just use dma_get_idx() for each
//	if (rom->dma_ready)
//		die_i("i can't dma_%s after dma_ready has been called", fname);
	
	/* swap start and end if they are not in ascending order */
	if (end < start)
	{
		int t = end;
		end = start;
		start = t;
	}
	
	/* we can't accept negative values */
	if (start < 0)
		die_i("how am i supposed to dma_%s index %d", fname, start);
	
	/* allocate compression buffers, 16 mb */
	if (!rom->mem.mb16)
		rom->mem.mb16 = malloc_safe(SIZE_16MB);
	if (!rom->mem.mb4)
		rom->mem.mb4 = malloc_safe(SIZE_4MB);
	
	/* no need to reencode when the codec is the same */
	if (!strcmp(from, to))
		return;
	
	/* no need to reencode when both are yaz */
	if (strstr(from, "yaz") && strstr(to, "yaz"))
		return;
	
	/* from codec */
	if (!strcmp(from, "yaz") || !strcmp(from, "slowyaz"))
	{
		from = "Yaz0";
		decfunc = yazdec;
	}
	else if (!strcmp(from, "raw"))
	{
		from = "raw0";
		decfunc = 0;
	}
	else
		die_i("dma_rearchive from='%s' unsupported", from);
	
	/* to codec */
	
	/* get encoding function */
	char *enc = to;
	if (!strcmp(enc, "yaz") || !strcmp(enc, "slowyaz"))
	{
		encfunc = yazenc;
		ctx_new = yazCtx_new;
		ctx_free = yazCtx_free;
	}
	//else if (!strcmp(enc, "slowyaz")) // no application for slowyaz anymore
	//	encfunc = slowyazenc;
	else if (!strcmp(enc, "lzo"))
	{
		encfunc = lzoenc;
		ctx_new = lzoCtx_new;
		ctx_free = lzoCtx_free;
	}
	else if (!strcmp(enc, "ucl"))
		encfunc = uclenc;
//	else if (!strcmp(enc, "zx7"))
//		encfunc = zx7enc;
	else if (!strcmp(enc, "aplib"))
		encfunc = aplenc;
	else
		die_i("i have never heard of '%s' compression", enc);
	
	/* allocate compression contexts (if applicable) */
	if (ctx_new)
	{
		ctx = ctx_new();
		if (!ctx)
			die_i("memory error");
	}
	
	/* this is inclusive, aka, start <= idx <= end */
	while (start <= end && start < rom->dma_num)
	{
		struct dma *dma = dma_get_idx(rom, start);
		
		unsigned int Osz = dma->end - dma->start;
		unsigned int Nsz;
		char name[32];
		char *errstr;
		unsigned char *dst = rom->data + dma->start;
		
		dma->writable = 0;
		dma->compress = 0;
		
		sprintf(name, "%08X", dma->start);
		
		errstr =
		yar_reencode(
			dst
			, Osz
			, rom->mem.mb16
			, &Nsz
			, 4 /* original yaz uses 12 */
			
			, name
			, from
			, rom->mem.mb4
			, ctx
			
			, decfunc
			, encfunc
			, 0
		);
		
		/* fatal error */
		if (errstr)
			die_i(errstr);
		
		/* new file won't fit */
		if (Nsz > Osz)
			die_i(
				"rearchiving failed, new archive 0x%X bytes too big"
				, Nsz - Osz
			);
		
		/* copy encoded file into rom */
		memcpy(dst, rom->mem.mb16, Nsz);
		
		/* file sizes changed */
		if (relocate)
			fprintf(stderr, "%.2f kb saved!\n", ((float)(Osz-Nsz))/1000);
		/* otherwise, just say it worked */
		else
			fprintf(stderr, "success!\n");
		
		/* update end to reflect saved space */
		/* TODO it is actually somehow necessary? */
		dma->end = dma->start + Nsz;
		//fprintf(stderr, " -> %08X %08X (%08X vs %08X)\n", dma->start, dma->end, Nsz, Osz);
		
		start += 1;
	}
	
	/* free compression contexts (if applicable) */
	if (ctx_free)
	{
		assert(ctx);
		ctx_free(ctx);
	}
}

/* apply cloudpatch to rom */
void
rom_cloudpatch(struct rom *rom, int ofs, char *fn)
{
	char *str;
	char *Ostr;
	unsigned int sz;
	unsigned char *dst;
	
	/* assertions */
	if (!rom || !rom->data)
		die_i("cannot cloudpatch uninitialized rom");
	
	if (ofs < 0 || ofs >= rom->data_sz)
		die_i("invalid cloudpatch offset %d (0x%X)", ofs, ofs);
	
	if (!fn)
		die_i("invalid cloudpatch name");
	
	/* load cloudpatch */
	Ostr = str = file_load(fn, &sz, 1);
	if (!str)
		die_i("something went wrong loading cloudpatch '%s'", fn);
	
	/* place cursor in rom */
	dst = rom->data + ofs;
	
	/* parse every line */
	while (1)
	{
		int cloudOfs;
		
		/* does not start with offset */
		if (memcmp(str, "0x", 2))
			goto nextline;
		
		/* failed to read offset for some reason */
		if (sscanf(str, "%X", &cloudOfs) != 1)
			goto nextline;
		
		/* go to comma */
		str = strchr(str, ',');
		if (!str)
			break;
		
		/* advance beyond comma */
		str++;
		
		/* now read xdigits until done */
		while (isxdigit(*str) && isxdigit(str[1]))
		{
			char ok[3] = {*str, str[1], '\0'};
			int x;
			
			if (sscanf(ok, "%02X", &x) != 1)
				goto nextline;
			
			dst[cloudOfs] = x;
			cloudOfs += 1;
			str += 2;
		}
		
nextline:
		/* advance to next line */
		if (!str || !*str)
			break;
		str = strchr(str, '\n');
		if (!str)
			break;
		/* skip whitespace */
		while (*str && *str <= ' ')
			str++;
		if (!*str)
			break;
	}
	
	/* cleanup */
	free(Ostr);
}


/****
 * folder functions
 ****/

/* allocate a folder structure and parse the requested directory;
   contents must be of one of the following forms:
    > "%d %s",   ex, "0 - gameplay_keep" or
    > "%s (%d)", ex, "gameplay_keep (0)" or
    > "%s_%d",   ex, "room_0"
   items are accessed in the order specified by the numerical part,
   which can be hexadecimal as long as it is preceeded by "0x"
   if ext is 0, it parses folders
   if ext is non-zero, it parses files of that extension, ex "zmap" */
struct folder *
folder_new(struct folder *folder, char *ext)
{
	wow_DIR *dir;
	struct wow_dirent *ep;
	struct fldr_item *item;
	char *path = ".";
	int count = 0;
	int recount = 0;
	int orderless = 0;
	int any_extension = 0;
	
#ifdef ZZRTL_NEW_TEST
	if (folder)
		die_i("i already initialized this folder");
#endif
	
	/* allocate a folder */
	folder = calloc_safe(1, sizeof(*folder));
	
	/* make lowercase copy of extension */
	if (ext)
	{
		char *oext = ext;
		
		/* skip any punctuation, making "*.zmap" be "zmap" */
		while (*ext == '*' || *ext == '/')
		{
			/* special control: doesn't fetch numbers nor index */
			if (*ext == '/')
			{
				/* second one enables fetching all extensions */
				if (orderless)
					any_extension = 1;
				/* first one disables ordering */
				orderless = 1;
			}
			ext++;
		}
		while (ispunct(*ext))
			ext++;
		/* the extension contains no alpha-numerics */
		if (!*ext)
			die_i("invalid extension '%s'", oext);
	}
	
#if 0
	/* default */
	if (!path)
		path = ".";
#endif
	
	/* first pass: count the contents */
	dir = wow_opendir(path);
	if (!dir)
		die_i("folder_new() failed to parse directory '%s'", path);
	while ((ep = wow_readdir(dir)))
		count += 1;
	wow_closedir(dir);
	
	/* folder is empty */
	if (!count)
		die_i("folder '%s' is empty", path);//return folder;
	
	/* allocate item array */
	item = calloc_safe(count, sizeof(*item));
	folder->item = item;
	folder->item_sz = count;
	
	/* second pass: retrieve requested contents */
	dir = wow_opendir(path);
	if (!dir)
		die_i("folder_new() failed to parse directory '%s'", path);
	for (
		recount = 0
		; (ep = wow_readdir(dir)) && recount < count
		; ++recount, ++item
	)
	{
		char *dn;
		int is_dir;
		
		dn = (char*)ep->d_name;
		is_dir = wow_is_dir(dn);
		
		/* every item gets initialized with -1 */
		item->index = -1;
		
		/* skip anything else starting with '.' or '_'
		   (includes "." and "..") */
		if (*dn == '.' || *dn == '_')
			continue;
		
		/* if we are grabbing files by extension, skip directories */
		if (ext && is_dir)
			continue;
		
		/* if we are grabbing folders, skip non-directories */
		if (!ext && !is_dir)
			continue;
		
		/* make a copy of the string */
		item->name = strdup_safe(dn);
		
		/* if grabbing by extension, confirm extension matches */
		if (ext && !any_extension)
		{
			char *ss;
			
			/* get extension */
			ss = strrchr(item->name, '.');
			
			/* skip period, etc */
			if (ss)
				while (*ss && ispunct(*ss))
					ss++;
			
			/* no extension, or mismatch; purge from list */
			/* invalid read size of 1 before; thanks valgrind! */
			if (!ss || (ss && strcasecmp(ss, ext)))
			{
				free(item->name);
				item->name = 0;
				continue;
			}
			
			/* but we want to ignore the extension for now */
			*ss = '\0';
		}
		
		/* retrieve values, making sure only one was found */
		if (!orderless)
		{
			/* if there isn't a value at the beginning, get the
			   last value that appears in the name */
			if(!str2val(item->name, &item->index, STR2VAL_ONLY_FIRST))
				str2val(
					item->name
					, &item->index
					, STR2VAL_ONLY_LAST | STR2VAL_WANT_ERRS
				);
		}
		
		/* if original string had extension, restore it now */
		if (ext)
			strcpy(item->name, dn);
	}
	
	if (recount != count)
		die_i("folder contents changed during read, try again");
	wow_closedir(dir);
	
	/* if we don't want them sorted, we can return early */
	if (orderless)
	{
		//fprintf(stderr, "orderless search = %d\n", count);
		folder->count = count;
		folder->remaining = count;
		return folder;
	}
	
	/* now sort folder item list, ascending */
	qsort(
		folder->item
		, count
		, sizeof(*folder->item)
		, fldr_item_ascending
	);
	
	/* print them all */
	for (item = folder->item; item - folder->item < count; ++item)
	{
		/* set active folder to the lowest used one */
		if (item->index >= 0 && !folder->active)
			folder->active = item;
		
		/* want highest index */
		folder->max = item->index;
		
		/* two folder items containing the same index */
		if (item->index == recount && item->index >= 0)
			die_i(
				"two folder items with the same index encountered:\n"
				"    '%s' vs '%s'"
				, (item-1)->name
				, item->name
			);
		
		/* debugging help */
#if 0
		fprintf(stdout, "[%d] %s\n", item->index, item->name);
#endif
		
		/* store current index as last index */
		recount = item->index;
	}
	
	folder->count = item - folder->active;
	folder->remaining = folder->count;
	
	DEBUG_PRINT("retrieved %d items\n", folder->count);
	
	return folder;
}

/* free a folder structure */
void
folder_free(struct folder *folder)
{
	if (!folder)
		return;
	
	/* folder contains items */
	if (folder->item)
	{
		struct fldr_item *item;
		
		/* walk item list, freeing resources owned by each */
		for (
			item = folder->item
			; item - folder->item < folder->item_sz
			; ++item
		)
		{
			if (item->name)
				free(item->name);
		}
		
		/* free item list */
		free(folder->item);
	}
	
	/* free folder */
	free(folder);
}

/* get current folder name */
char *
folder_name(struct folder *folder)
{
	if (!folder || !folder->active)
		die_i("i can't retrieve name from uninitialized folder");
	
	if (!folder->remaining)
		die_i("end of folder list exceeded");
	
	return folder->active->name;
}

/* get numerical part of current folder name */
int
folder_index(struct folder *folder)
{
	if (!folder || !folder->active)
		die_i("i can't retrieve index from uninitialized folder");
	
	if (!folder->remaining)
		die_i("end of folder list exceeded");
	
	return folder->active->index;
}

/* go to next folder in list; returns 0 when end of list is reached */
int
folder_next(struct folder *folder)
{
	if (!folder || !folder->active)
		die_i("i can't go to next from uninitialized folder");
	
	/* reached end of list */
	if (!folder->remaining)
		return 0;
	
	/* go to next in list */
	folder->active += 1;
	
	/* decrement counter */
	folder->remaining -= 1;
	
	return 1;
}

/* get number of items in folder list */
int
folder_count(struct folder *folder)
{
	if (!folder)
		die_i("i can't retrieve count from uninitialized folder");
	
	if (!folder->active)
		return 0;
	
	return folder->count;
}

/* get number of items between current and end */
int
folder_remaining(struct folder *folder)
{
	if (!folder)
		die_i("i can't retrieve remaining from uninitialized folder");
	
	if (!folder->active)
		return 0;
	
	return folder->remaining;
}

/* get highest index detected in list */
int
folder_max(struct folder *folder)
{
	if (!folder)
		die_i("i can't retrieve max from uninitialized folder");
	
	if (!folder->active)
		return 0;
	
	return folder->max;
}

/* sort struct fldr_item array by ascending index */
int
fldr_item_ascending(const void *_a, const void *_b)
{
	const struct fldr_item *a = _a;
	const struct fldr_item *b = _b;
	
	if (a->index < b->index)
		return -1;
	else if (a->index > b->index)
		return 1;
	return 0;
	
	//return a->index > b->index;
}

/* locate folder item by name */
struct fldr_item *
folder_find_name(struct folder *folder, char *name)
{
	struct fldr_item *item;
	
	assert(folder);
	assert(name);
	
	for (
		item = folder->item
		; item - folder->item < folder->item_sz
		; ++item
	)
	{
		if (item->name && !strcmp(item->name, name))
			return item;
	}
	
	return 0;
}

/* locate folder item by name, ignoring extension */
struct fldr_item *
folder_find_name_n_ext(struct folder *folder, char *name)
{
	struct fldr_item *item;
	
	assert(folder);
	assert(name);
	
	for (
		item = folder->item
		; item - folder->item < folder->item_sz
		; ++item
	)
	{
		char *period;
		int nchar;
		
		/* item has no name */
		if (!item->name)
			continue;
		
		/* doesn't contain a period */
		if (!(period = strrchr(item->name, '.')))
			continue;
		
		/* number of bytes to compare */
		nchar = period - item->name;
		
		/* names match */
		if (!memcmp(item->name, name, nchar))
			return item;
	}
	
	return 0;
}


/****
 * conf functions
 ****/

/* allocate a conf structure */
struct conf *
conf_new(struct conf *conf, char *fn, char *format)
{
	void *o_contig;
	char *str;
	struct conf_item *item;
	unsigned int sz;
	enum conf_fmt fmt = 0;
	
#ifdef ZZRTL_NEW_TEST
	if (conf)
		die_i("i already initialized this conf");
#endif

	if (!strcmp(format, "table"))
		fmt = CONF_FMT_TABLE;
	else if (!strcmp(format, "list"))
		fmt = CONF_FMT_LIST;
	else
		format = 0; /* invalid format */
	
	if (!fn)
		die_i("conf_new() no fn provided");
	
	if (!format)
		die_i("conf_new() invalid format");
	
	/* if fn starts with '*', grab by extension */
	fn = find_extension(fn, 1);
	
	conf = calloc_safe(1, sizeof(*conf));
	
	/* store format */
	conf->fmt = fmt;
	
	/* propagate conf file */
	str = file_load(fn, &sz, 1);
	if (!str)
		die_i("something went wrong loading conf '%s'", fn);
	str[sz] = '\0';
	conf->raw = str;
	
	/* replace comments with whitespace */
	str = preproc_remove_comments(str, 0);
	
	/* back up contig with preproc_getcontig() */
	o_contig = preproc_getcontig();
	
	/* override with custom contig test */
	preproc_usecontig(iscontig);
	
	/* table parser */
	if (fmt == CONF_FMT_TABLE)
	{
		/* count names */
		for (str = conf->raw; *str; str += sz)
		{
			/* get length of current token */
			sz = preproc_toklen(str);
			
			/* if is whitespace */
			if (*str <= ' ')
			{
				/* if whitespace block contains newline, we're done */
				if (memchr(str, '\n', sz))
					break;
				
				/* no need to parse whitespace */
				continue;
			}
			
			/* increment number of known names */
			conf->item_sz += 1;
		}
		
		/* now allocate name list */
		DEBUG_PRINT("alloc %d items\n", conf->item_sz);
		item = calloc_safe(conf->item_sz, sizeof(*item));
		conf->item = item;
		
		/* now propagate name list */
		for (str = conf->raw; *str; str += sz)
		{
			/* get length of current token */
			sz = preproc_toklen(str);
			
			/* if is whitespace */
			if (*str <= ' ')
			{
				int contains_newline;
				
				/* test this before modifying contents */
				contains_newline = !!memchr(str, '\n', sz);
				
				/* ensures previous token is zero-terminated */
				*str = '\0';
				
				/* if whitespace block contains newline, we're done */
				if (contains_newline)
				{
					str += sz;
					break;
				}
				
				/* no need to parse whitespace */
				continue;
			}
			
			/* make sure item is in range */
			if (item - conf->item >= conf->item_sz)
				die_i("something went wrong parsing table '%s'", fn);
			
			/* add token to name list */
			item->name = str;
			
			/* if it is a quote, strip quotes */
			strip_quotes(&item->name, sz);
#if 0
			if (*str == '"' || *str == '\'')
			{
				item->name += 1;
				str[sz-1] = '\0';
			}
#endif
			
			/* move on to next item in list */
			item += 1;
		}
		
		/* at this point, str should be situated at next row */
		conf->line = str;
		
#ifndef NDEBUG
		/* debugging help */
		for (
			item = conf->item
			; item - conf->item < conf->item_sz
			; ++item
		)
			DEBUG_PRINT("[%d] %s\n", item - conf->item, item->name);
#endif
		
		/* propagate values with next row's contents */
		conf_next(conf);
	}
	
	/* list parser */
	else if (fmt == CONF_FMT_LIST)
	{
		/* count non-whitespace tokens */
		for (str = conf->raw; *str; str += sz)
		{
			/* get length of current token */
			sz = preproc_toklen(str);
			
			/* skip whitespace */
			if (*str <= ' ')
				continue;
			
			conf->item_sz += 1;
		}
		
		/* num items should be even number, as they come in pairs */
		if (conf->item_sz & 1)
			die_i("list '%s' formatted badly", fn);
		conf->item_sz /= 2;
		
		/* alloc items */
		item = calloc_safe(conf->item_sz, sizeof(*item));
		conf->item = item;
		
		/* now propagate name list */
		for (str = conf->raw; *str; str += sz)
		{
			/* get length of current token */
			sz = preproc_toklen(str);
			
			/* is whitespace */
			if (*str <= ' ')
			{
				/* ensures whatever token came before is 0-term'd */
				*str = '\0';
				
				/* skip whitespace */
				continue;
			}
			
			/* item name points to first token found */
			if (!item->name)
			{
				item->name = str;
				strip_quotes(&item->name, sz);
			}
			
			/* item value points to next token found */
			else if (!item->value)
			{
				item->value = str;
				strip_quotes(&item->value, sz);
			
				/* and we advance to next item */
				item += 1;
			}
		}
		
		/* r3: enable conf_remaining() and conf_next() in list types */
		conf->remaining = conf->item_sz;
		conf->list_walk = conf->item;
		
#ifndef NDEBUG
		DEBUG_PRINT("loaded list '%s':\n", fn);
		/* debugging help */
		for (
			item = conf->item
			; item - conf->item < conf->item_sz
			; ++item
		)
			DEBUG_PRINT("[%d] %s\n", item - conf->item, item->name);
#endif
	}
	
	/* restore previous contig test */
	preproc_usecontig(o_contig);
	
	return conf;
}

/* free a conf structure */
void
conf_free(struct conf *conf)
{
	if (!conf)
		return;
	
	if (conf->raw)
		free(conf->raw);
	
	/* no need to free list items separately, since
	   they all point to data within conf->raw */
	if (conf->item)
		free(conf->item);
}

/* get integer value associated with name */
int
conf_get_int(struct conf *conf, char *name)
{
	struct conf_item *item;
	int val;
	
	if (!conf)
		return 0;
	
	item = conf_item_find_error(conf, name);
	
	str2val(item->value, &val, STR2VAL_WANT_ERRS);
	
	return val;
}

/* get pointer to string associated with name */
char *
conf_get_str(struct conf *conf, char *name)
{
	struct conf_item *item;
	
	if (!conf)
		return 0;
	
	item = conf_item_find_error(conf, name);
	
	return item->value;
}

/* r3 */
/* get name of selected list item (lists only) */
/* returns 0 at end of list */
char *
conf_name(struct conf *conf)
{
	if (!conf)
		return 0;
	
	if (conf->fmt != CONF_FMT_LIST)
		die_i("conf_name() used with non-list type");
	
	if (!conf->remaining)
		return 0;
	
	return conf->list_walk->name;
}

/* r3 */
/* get value of selected list item, as string (lists only) */
/* returns 0 at end of list */
char *
conf_value(struct conf *conf)
{
	if (!conf)
		return 0;
	
	if (conf->fmt != CONF_FMT_LIST)
		die_i("conf_value() used with non-list type");
	
	if (!conf->remaining)
		return 0;
	
	return conf->list_walk->value;
}

/* in the case of a table, go to next line;
   returns 0 when there is no next line */
int
conf_next(struct conf *conf)
{
	void *o_contig;
	struct conf_item *item;
	char *str;
	int sz;
	
	/* r3: enable conf_next() in list types */
	/* next item in list format */
	if (conf && conf->fmt == CONF_FMT_LIST)
	{
		if (!conf->remaining)
			return 0;
		conf->remaining -= 1;
		conf->list_walk += 1;
		return 1;
	}
	
	if (!conf || !conf->line)
		die_i("can't conf_next() an uninitialized conf");
	
	if (conf->fmt != CONF_FMT_TABLE)
		die_i("can't conf_next() in a conf list, must be table");
	
	/* if at end of string, this will be 0, aka, no next line */
	conf->remaining = *conf->line;
	
	/* end of string */
	if (!conf->remaining)
		return 0;
	
	/* back up contig with preproc_getcontig() */
	o_contig = preproc_getcontig();
	
	/* override with custom contig test */
	preproc_usecontig(iscontig);
	
	/* parse every item on the line */
	item = conf->item;	
	for (str = conf->line; *str; str += sz)
	{
		/* get length of current token */
		sz = preproc_toklen(str);
		
		/* if is whitespace */
		if (*str <= ' ')
		{
			int contains_newline;
			
			/* test this before modifying contents */
			contains_newline = !!memchr(str, '\n', sz);
			
			/* ensures previous token is zero-terminated */
			*str = '\0';
			
			/* if whitespace block contains newline, we're done */
			if (contains_newline)
			{
				str += sz;
				break;
			}
			
			/* no need to parse whitespace */
			continue;
		}
			
		/* make sure item is in range */
		if (item - conf->item >= conf->item_sz)
			die_i("something went wrong parsing table");
			
		/* add token to name list */
		item->value = str;
		
		/* if it is a quote, strip quotes */
		strip_quotes(&item->value, sz);
#if 0
		if (*str == '"' || *str == '\'')
		{
			item->value += 1;
			str[sz-1] = '\0';
		}
#endif
		
		/* go to next item in list */
		item += 1;
	}
	
	/* restore previous contig test */
	preproc_usecontig(o_contig);
	
	/* start on the next line the next time this is called */
	conf->line = str;
	
#ifndef NDEBUG
		/* debugging help */
		DEBUG_PRINT("conf_next() completed, result displayed:\n");
		for (
			item = conf->item
			; item - conf->item < conf->item_sz
			; ++item
		)
			DEBUG_PRINT(
				"[%d] %s: %s\n"
				, item - conf->item
				, item->name
				, item->value
			);
#endif
	
	return conf->remaining;
}

/* in the case of a table, this will be non-zero if there are
   rows remaining, and 0 otherwise */
int
conf_remaining(struct conf *conf)
{
	if (!conf)
		die_i("can't conf_remaining() an uninitialized conf");
	
	/* r3: enable conf_next() in list types */
	assert(conf->fmt < CONF_FMT_MAX);
	
//	if (conf->fmt != CONF_FMT_TABLE)
//		die_i("can't conf_remaining() in a conf list, must be table");
	
	return conf->remaining;
}

/* returns non-zero if a name can be found inside a conf structure */
int
conf_exists(struct conf *conf, char *fn)
{
	return !!conf_item_find(conf, fn);
}

#ifdef ZZRTL_TEST
int
main(void)
{
	return 0;
}
#endif /* ZZRTL_TEST */

