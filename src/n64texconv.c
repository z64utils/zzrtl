#include "n64texconv.h"
#include <math.h>
#include <assert.h>
#include <stdint.h> /* uint32_t */
#include <string.h> /* memset */
#include <stdio.h>

struct vec4b {
	unsigned char x;
	unsigned char y;
	unsigned char z;
	unsigned char w;
};

struct vec4b_2n64 {
	unsigned char x;
	unsigned char y;
	unsigned char z;
	unsigned char w;
};

#define CONV_7(x)   lut_7[x]
#define CONV_15(x)  lut_15[x]
#define CONV_31(x)  lut_31[x]
#define CONV_255(x) x

static const unsigned char lut_7[] =
{
	0, 36, 73, 109, 146, 182, 219, 255
};

static const unsigned char lut_15[] =
{
	0, 17, 34, 51, 68, 85, 102, 119,
	136, 153, 170, 187, 204, 221, 238, 255
};

static const unsigned char lut_31[] =
{
	0, 8, 16, 25, 33, 41, 49, 58, 66,
	74, 82, 90, 99, 107, 115, 123, 132,
	140, 148, 156, 165, 173, 181, 189,
	197, 206, 214, 222, 230, 239, 247, 255
};

static
inline
int
imn(int a, int b)
{
	return (a < b) ? a : b;
}

static
inline
int
imx(int a, int b)
{
	return (a > b) ? a : b;
}

#define N64_COLOR_FUNC_NAME_TO(FMT) v4b_to_##FMT

#define N64_COLOR_FUNC_NAME(FMT) v4b_from_##FMT

#define N64_COLOR_FUNC(FMT) \
static \
inline \
void \
N64_COLOR_FUNC_NAME(FMT)( \
	struct vec4b *color \
	, unsigned char *b \
)

#define N64_COLOR_FUNC_TO(FMT) \
static \
inline \
void \
N64_COLOR_FUNC_NAME_TO(FMT)( \
	struct vec4b_2n64 *color \
	, unsigned char *b \
)

/* from formats */
N64_COLOR_FUNC(i4)
{
	color->x = CONV_15(*b);
	color->y = color->x;
	color->z = color->x;
	color->w = color->x;// * 0.5; /* TODO: * 0.5 fixes kokiri path */
}


N64_COLOR_FUNC(ia4)
{
	color->x = CONV_7(*b >> 1);
	color->y = color->x;
	color->z = color->x;
	color->w = ((*b) & 1) * 255;
}


N64_COLOR_FUNC(i8)
{
	color->x = CONV_255(*b);
	color->y = color->x;
	color->z = color->x;
	color->w = color->x;
}


N64_COLOR_FUNC(ia8)
{
	color->x = CONV_15(*b >> 4);
	color->y = color->x;
	color->z = color->x;
	color->w = CONV_15(*b & 0xF);
}


N64_COLOR_FUNC(ia16)
{
	color->x = CONV_255(*b);
	color->y = color->x;
	color->z = color->x;
	color->w = CONV_255(b[1]);
}


N64_COLOR_FUNC(rgba5551)
{
	int comb = (b[0]<<8)|b[1];
	color->x = CONV_31((comb >> 11) & 0x1F);
	color->y = CONV_31((comb >>  6) & 0x1F);
	color->z = CONV_31((comb >>  1) & 0x1F);
	color->w = (comb & 1) * 255;
}


N64_COLOR_FUNC(ci8)
{
	/* note that this is just rgba5551 */
	int comb = (b[0]<<8)|b[1];
	color->x = CONV_31((comb >> 11) & 0x1F);
	color->y = CONV_31((comb >>  6) & 0x1F);
	color->z = CONV_31((comb >>  1) & 0x1F);
	color->w = (comb & 1) * 255;
}


N64_COLOR_FUNC(ci4)
{
	/* note that this is just rgba5551 */
	int comb = (b[0]<<8)|b[1];
	color->x = CONV_31((comb >> 11) & 0x1F);
	color->y = CONV_31((comb >>  6) & 0x1F);
	color->z = CONV_31((comb >>  1) & 0x1F);
	color->w = (comb & 1) * 255;
}


N64_COLOR_FUNC(rgba8888)
{
	color->x = CONV_255(b[0]);
	color->y = CONV_255(b[1]);
	color->z = CONV_255(b[2]);
	color->w = CONV_255(b[3]);
}

/* 1bit */
N64_COLOR_FUNC(onebit)
{
	int i = 8;
	for (i = 0; i < 8; ++i)
	{
		if (((*b)>>i)&1)
		{
			color->x = 0xFF;
			color->y = 0xFF;
			color->z = 0xFF;
			color->w = 0xFF;
		}
		else
		{
			color->x = 0;
			color->y = 0;
			color->z = 0;
			color->w = 0xFF;
		}
		--color;
	}
}

/**************************************************************/

/* to formats */
N64_COLOR_FUNC_TO(i4)
{
	/* TUNING could omit float math altogether and just use (x & 15) */
	float f = color->x * 0.003921569f;
	int i;
	f *= 15;
	i = round(f);
	*b = i;
}


N64_COLOR_FUNC_TO(ia4)
{
	/* TUNING could omit float math altogether and just use (x & 7) */
	float f = color->x * 0.003921569f;
	int i;
	f *= 7;
	i = round(f);
	*b = (i << 1) | ((color->w & 0x80) > 1);
//	*b = (color->x & 0xFE) | ((color->w & 0x80) > 1);
}


N64_COLOR_FUNC_TO(i8)
{
	*b = color->x;
}


N64_COLOR_FUNC_TO(ia8)
{
	/* TUNING could omit float math altogether and just use (x << 4) */
	float fX = color->x * 0.003921569f;
	float fW = color->w * 0.003921569f;
	int iX;
	int iW;
	fX *= 15;
	iX = round(fX);
	fW *= 15;
	iW = round(fW);
//	*b = (iX << 4) | (color->w & 0xF);
	*b = (iX << 4) | (iW);
}


N64_COLOR_FUNC_TO(ia16)
{
	b[0] = color->x;
	b[1] = color->w;
}

N64_COLOR_FUNC_TO(rgba5551)
{
	int comb = 0;
	
	comb |= (color->x & 0xF8) << 8; // r
	comb |= (color->y & 0xF8) << 3; // g
	comb |= (color->z & 0xF8) >> 2; // b
	comb |= (color->w & 0x80)  > 1; // a
	
	b[0] = comb >> 8;
	b[1] = comb;
}


N64_COLOR_FUNC_TO(ci8)
{
	/* TODO ci conversion */
}


N64_COLOR_FUNC_TO(ci4)
{
	/* TODO ci conversion */
}


N64_COLOR_FUNC_TO(rgba8888)
{
	b[0] = CONV_255(color->x);
	b[1] = CONV_255(color->y);
	b[2] = CONV_255(color->z);
	b[3] = CONV_255(color->w);
}


static
inline
void
vec4b_2n64_from_rgba5551(struct vec4b_2n64 *color, unsigned char b[2])
{
	int comb = (b[0]<<8)|b[1];
	color->x = CONV_31((comb >> 11) & 0x1F);
	color->y = CONV_31((comb >>  6) & 0x1F);
	color->z = CONV_31((comb >>  1) & 0x1F);
	color->w = (comb & 1) * 255;
}


/* converter array */
static
void
(*n64_colorfunc_array[])(
	struct vec4b *color
	, unsigned char *b
) = {
	/* rgba = 0 */
	0,                             /*  4b */
	0,                             /*  8b */
	N64_COLOR_FUNC_NAME(rgba5551), /* 16b */
	N64_COLOR_FUNC_NAME(rgba8888), /* 32b */
	
	/* yuv = 1 */
	0,                             /*  4b */
	0,                             /*  8b */
	0,                             /* 16b */
	0,                             /* 32b */
	
	/* ci = 2 */
	N64_COLOR_FUNC_NAME(ci4),      /*  4b */
	N64_COLOR_FUNC_NAME(ci8),      /*  8b */
	0,                             /* 16b */
	0,                             /* 32b */
	
	/* ia = 3 */
	N64_COLOR_FUNC_NAME(ia4),      /*  4b */
	N64_COLOR_FUNC_NAME(ia8),      /*  8b */
	N64_COLOR_FUNC_NAME(ia16),     /* 16b */
	0,                             /* 32b */
	
	/* i = 4 */
	N64_COLOR_FUNC_NAME(i4),       /*  4b */
	N64_COLOR_FUNC_NAME(i8),       /*  8b */
	0,                             /* 16b */
	0,                             /* 32b */
	
	/* 1bit = 5 */
	N64_COLOR_FUNC_NAME(onebit),   /*  4b */
	N64_COLOR_FUNC_NAME(onebit),   /*  8b */
	N64_COLOR_FUNC_NAME(onebit),   /* 16b */
	N64_COLOR_FUNC_NAME(onebit)    /* 32b */
};


/* converter array (to n64 format) */
static
void
(*n64_colorfunc_array_to[])(
	struct vec4b_2n64 *color
	, unsigned char *b
) = {
	/* rgba = 0 */
	0,                                /*  4b */
	0,                                /*  8b */
	N64_COLOR_FUNC_NAME_TO(rgba5551), /* 16b */
	N64_COLOR_FUNC_NAME_TO(rgba8888), /* 32b */
	
	/* yuv = 1 */
	0,                                /*  4b */
	0,                                /*  8b */
	0,                                /* 16b */
	0,                                /* 32b */
	
	/* ci = 2 */
	N64_COLOR_FUNC_NAME_TO(ci4),      /*  4b */
	N64_COLOR_FUNC_NAME_TO(ci8),      /*  8b */
	0,                                /* 16b */
	0,                                /* 32b */
	
	/* ia = 3 */
	N64_COLOR_FUNC_NAME_TO(ia4),      /*  4b */
	N64_COLOR_FUNC_NAME_TO(ia8),      /*  8b */
	N64_COLOR_FUNC_NAME_TO(ia16),     /* 16b */
	0,                                /* 32b */
	
	/* i = 4 */
	N64_COLOR_FUNC_NAME_TO(i4),       /*  4b */
	N64_COLOR_FUNC_NAME_TO(i8),       /*  8b */
	0,                                /* 16b */
	0                                 /* 32b */
};


static
inline
int
diff_int(int a, int b)
{
	if (a < b)
		return b - a;
	
	return a - b;
}


static
inline
unsigned int
get_size_bytes(int w, int h, int is_1bit, enum n64texconv_bpp bpp)
{
	/* 1bit per pixel */
	if (is_1bit)
		return (w * h) / 8;
	
	switch (bpp)
	{
		case N64TEXCONV_4:  return (w * h) / 2;
		case N64TEXCONV_8:  return w * h;
		case N64TEXCONV_16: return w * h * 2;
		case N64TEXCONV_32: return w * h * 4;
	}
	
	/* 1/2 byte per pixel */
	if (bpp == N64TEXCONV_4)
		return (w * h) / 2;
	
	/* 4 bytes per pixel */
	if (bpp == N64TEXCONV_32)
		return w * h * 4;
	
	/* 1 or 2 bytes per pixel */
	return w * h * bpp;
}


static
inline
void
texture_to_rgba8888(
	void n64_colorfunc(struct vec4b *color, unsigned char *b)
	, unsigned char *dst
	, unsigned char *pix
	, unsigned char *pal
	, int is_ci
	, enum n64texconv_bpp bpp
	, int w
	, int h
)
{
	struct vec4b *color = (struct vec4b*)dst;
	int is_4bit = (bpp == N64TEXCONV_4);
	int alt = 1;
	int i;
	unsigned int sz;
	int is_1bit = 0;//(n64_colorfunc == N64_COLOR_FUNC_NAME(onebit));
	
	/* determine resulting size */
	sz = get_size_bytes(w, h, is_1bit, bpp);
	
	/* work backwards from end */
	color += w * h;
	pix += sz;
	
#if 0
	if (is_1bit)
	{
		/* this converter operates on 8 px at a time */
		/* start with last pixel */
		--color;
		for (i=0; i < w * h; i += 8)
		{
			/* pixel advancement logic */
			--pix;
			
			/* convert pixel */
			n64_colorfunc(color, pix);
			color -= 8;
		}
		return;
	}
#endif
	
	for (i=0; i < w * h; ++i)
	{
		/* pixel advancement logic */
		/* 4bpp */
		if (is_4bit)
			pix -= alt & 1;
		/* 32bpp */
		else if (bpp == N64TEXCONV_32)
			pix -= 4;
		/* 8bpp and 16bpp */
		else
			pix -= bpp;
		--color;
		
		unsigned char *b = pix;
		unsigned char  c;
		
		/* 4bpp setup */
		if (is_4bit)
		{
			c = *b;
			b = &c;
			if (!(alt&1))
				c >>=  4;
			else
				c &=  15;
		}
		
		/* color-indexed */
		if (is_ci)
		{
			/* the * 2 is b/c ci textures have 16-bit color palettes */
			
			/* 4bpp */
			if (is_4bit)
				b = pal + c * 2;
			else
				b = pal + *pix * 2;
		}
		
		/* convert pixel */
		n64_colorfunc(color, b);
		
		++alt;
	}
}


static
inline
void
texture_to_n64(
	void n64_colorfunc(struct vec4b_2n64 *color, unsigned char *b)
	, unsigned char *dst
	, unsigned char *pix
	, unsigned char *pal
	, int pal_colors
	, int is_ci
	, enum n64texconv_bpp bpp
	, int w
	, int h
	, unsigned int *sz
)
{
	/* color points to last color */
	struct vec4b_2n64 *color = (struct vec4b_2n64*)(pix);
	int is_4bit = (bpp == N64TEXCONV_4);
	int alt = 0;
	int i;
	if (is_4bit && pal_colors > 16)
		pal_colors = 16;
	
	/* determine resulting size */
	*sz = get_size_bytes(w, h, 0/*FIXME*/, bpp);
	
	/* already in desirable format */
	if (bpp == N64TEXCONV_32)
		return;
	
	for (i=0; i < w * h; ++i, ++color)
	{
		unsigned char *b = dst;
		unsigned char  c;
		
		/* 4bpp setup */
		if (is_4bit)
		{
			c = *b;
			b = &c;
		}
		/* color-indexed */
		if (is_ci)
		{
			/* the * 2 is b/c ci textures have 16-bit color palettes */
			struct vec4b_2n64 Pcolor;
			struct vec4b_2n64 nearest = {255, 255, 255, 255};
			int nearest_idx = 0;
			int margin = 4; /* margin of error allowed */
			int Pidx;
			
			/* this is confirmed working on alpha pixels; try *
			 * importing eyes-xlu.png with palette 0x5C00 in  *
			 * object_link_boy.zobj                           */
			
			/* step through every color in the palette */
			for (Pidx = 0; Pidx < pal_colors; ++Pidx)
			{
				vec4b_2n64_from_rgba5551(&Pcolor, pal + Pidx * 2);
				
				/* measure difference between colors */
				struct vec4b_2n64 diff = {
					diff_int(Pcolor.x, color->x)
					, diff_int(Pcolor.y, color->y)
					, diff_int(Pcolor.z, color->z)
					, diff_int(Pcolor.w, color->w)
				};
				
				/* new nearest color */
				if (diff.x <= margin + nearest.x
					&& diff.y <= margin + nearest.y
					&& diff.z <= margin + nearest.z
					&& diff.w <= margin + nearest.w
				)
				{
					nearest_idx = Pidx;
					nearest = diff;
					
					/* exact match, safe to break */
					if (diff.x == 0
						&& diff.y == 0
						&& diff.z == 0
						&& diff.w == 0
					)
						break;
				}
			}
			
			/* 4bpp */
			if (is_4bit)
				c = nearest_idx;
			else
				*dst = nearest_idx;
		}
		
		/* convert pixel */
		else
			n64_colorfunc(color, b);
		
		/* 4bpp specific */
		if (is_4bit)
		{
			/* clear when we initially find it */
			if (!(alt&1))
				*dst = 0;
			
			/* how to transform result */
			if (!(alt&1))
				c <<=  4;
			else
				c &=  15;
			
			*dst |= c;
		}
		
		/* pixel advancement logic */
		/* 4bpp */
		if (is_4bit)
			dst += ((alt++)&1);
		/* 32bpp */
		else if (bpp == N64TEXCONV_32)
			dst += 4;
		/* 8bpp and 16bpp */
		else
			dst += bpp;
	}
}


const char *
n64texconv_to_rgba8888(
	unsigned char *dst
	, unsigned char *pix
	, unsigned char *pal
	, enum n64texconv_fmt fmt
	, enum n64texconv_bpp bpp
	, int w
	, int h
)
{
	/* error strings this function can return */
	static const char errstr_badformat[]  = "invalid format";
	static const char errstr_dimensions[] = "invalid dimensions (<= 0)";
	static const char errstr_palette[]    = "no palette";
	static const char errstr_texture[]    = "no texture";
	static const char errstr_dst[]        = "no destination buffer";

	/* no destination buffer */
	if (!dst)
		return errstr_dst;
	
	/* no texture */
	if (!pix)
		return errstr_texture;
	
	/* invalid format */
	if (
		fmt >= N64TEXCONV_FMT_MAX
		|| bpp > N64TEXCONV_32
		|| n64_colorfunc_array[fmt * 4 + bpp] == 0
	)
		return errstr_badformat;
	
	/* invalid dimensions (are <= 0) */
	if (w <= 0 || h <= 0)
		return errstr_dimensions;
	
	/* no palette to accompany ci texture data */
	if (fmt == N64TEXCONV_CI && pal == 0)
		return errstr_palette;
	
	/* convert texture using appropriate pixel converter */
	texture_to_rgba8888(
		n64_colorfunc_array[fmt * 4 + bpp]
		, dst
		, pix
		, pal
		, fmt == N64TEXCONV_CI
		, bpp
		, w
		, h
	);
	
	/* success */
	return 0;
}


const char *
n64texconv_to_n64(
	unsigned char *dst
	, unsigned char *pix
	, unsigned char *pal
	, int pal_colors
	, enum n64texconv_fmt fmt
	, enum n64texconv_bpp bpp
	, int w
	, int h
	, unsigned int *sz
)
{
	/* error strings this function can return */
	static const char errstr_badformat[]  = "invalid format";
	static const char errstr_dimensions[] = "invalid dimensions (<= 0)";
	static const char errstr_dst[]        = "no buffer";
	static const char errstr_palette[]    = "no palette";
	
	unsigned int sz_unused;

	/* no src/dst buffers defined */
	if (!dst || !pix)
		return errstr_dst;
	
	/* sz */
	if (!sz)
		sz = &sz_unused;
	
	/* invalid format */
	if (
		fmt >= N64TEXCONV_FMT_MAX
		|| bpp > N64TEXCONV_32
		|| n64_colorfunc_array[fmt * 4 + bpp] == 0
	)
		return errstr_badformat;
	
	/* ci format requires pal and pal_colors */
	if (fmt == N64TEXCONV_CI && (!pal || pal_colors <= 0))
		return "ci format but no palette provided";
	
	/* invalid dimensions (are <= 0) */
	if (w <= 0 || h <= 0)
		return errstr_dimensions;
	
	/* no palette to accompany ci texture data */
	if (fmt == N64TEXCONV_CI && pal == 0)
		return errstr_palette;
	
	/* convert texture using appropriate pixel converter */
	texture_to_n64(
		n64_colorfunc_array_to[fmt * 4 + bpp]
		, dst
		, pix
		, pal
		, pal_colors
		, fmt == N64TEXCONV_CI
		, bpp
		, w
		, h
		, sz
	);
	
	/* success */
	return 0;
}


const char *
n64texconv_to_n64_and_back(
	unsigned char *pix
	, unsigned char *pal
	, int pal_colors
	, enum n64texconv_fmt fmt
	, enum n64texconv_bpp bpp
	, int w
	, int h
)
{
	const char *err;
	
	err = n64texconv_to_n64(pix, pix, pal, pal_colors, fmt, bpp, w, h, 0);
	if (err)
		return err;
	
	err = n64texconv_to_rgba8888(pix, pix, pal, fmt, bpp, w, h);
	if (err)
		return err;
	
	return 0;
}


/* lifted from colorquantize.c */
 
#define ON_INHEAP	1

typedef struct oct_node_t oct_node_t, *oct_node;
struct oct_node_t{
	int64_t r, g, b; /* sum of all child node colors */
	int count, heap_idx;
	unsigned char n_kids, kid_idx, flags, depth;
	oct_node kids[8], parent;
};

typedef struct {
	int alloc, n;
	oct_node* buf;
} node_heap;

struct pqueue
{
	void   *pix;
	void   *next;
	int     w;
	int     h;
	int     dither;
};

struct n64texconv_palctx
{
	oct_node       pool;
	oct_node       root;
	node_heap      heap;
	void        *(*realloc)(void *, size_t);
	void        *(*calloc)(size_t, size_t);
	void         (*free)(void *);
	int            pool_len;
	int            n_colors;  /* max colors */
	int            n_alpha;   /* num alpha colors */
	void          *colors;    /* where colors end up stored (rgba32) */
	struct pqueue *queue;     /* teimagexture queue */
};

static
/*inline */int cmp_node(oct_node a, oct_node b) // why the inline?
{
	if (a->n_kids < b->n_kids) return -1;
	if (a->n_kids > b->n_kids) return 1;
	
	int ac = a->count >> a->depth;
	int bc = b->count >> b->depth;
	return ac < bc ? -1 : ac > bc;
}

static
void
down_heap(node_heap *h, oct_node p)
{
	int n = p->heap_idx, m;
	while (1) {
		m = n * 2;
		if (m >= h->n) break;
		if (m + 1 < h->n && cmp_node(h->buf[m], h->buf[m + 1]) > 0) m++;

		if (cmp_node(p, h->buf[m]) <= 0) break;

		h->buf[n] = h->buf[m];
		h->buf[n]->heap_idx = n;
		n = m;
	}
	h->buf[n] = p;
	p->heap_idx = n;
}

static
void
up_heap(node_heap *h, oct_node p)
{
	int n = p->heap_idx;
	oct_node prev;

	while (n > 1) {
		prev = h->buf[n / 2];
		if (cmp_node(p, prev) >= 0) break;

		h->buf[n] = prev;
		prev->heap_idx = n;
		n /= 2;
	}
	h->buf[n] = p;
	p->heap_idx = n;
}


static
void
heap_add(
	void *realloc(void *, size_t)
	, node_heap *h
	, oct_node p
)
{
	if ((p->flags & ON_INHEAP)) {
		down_heap(h, p);
		up_heap(h, p);
		return;
	}

	p->flags |= ON_INHEAP;
	if (!h->n) h->n = 1;
	if (h->n >= h->alloc) {
		while (h->n >= h->alloc) h->alloc += 1024;
		h->buf = realloc(h->buf, sizeof(oct_node) * h->alloc);
	}

	p->heap_idx = h->n;
	h->buf[h->n++] = p;
	up_heap(h, p);
}

static
oct_node
pop_heap(node_heap *h)
{
	if (h->n <= 1)
		return 0;

	oct_node ret = h->buf[1];
	h->buf[1] = h->buf[--h->n];

	h->buf[h->n] = 0;

	h->buf[1]->heap_idx = 1;
	down_heap(h, h->buf[1]);

	return ret;
}


static
oct_node
node_new(
	struct n64texconv_palctx *ctx
	, unsigned char idx
	, unsigned char depth
	, oct_node p
)
{
	if (ctx->pool_len <= 1) {
		oct_node p = ctx->calloc(sizeof(oct_node_t), 2048);
		p->parent = ctx->pool;
		ctx->pool = p;
		ctx->pool_len = 2047;
	}

	oct_node x = ctx->pool + ctx->pool_len--;
	x->kid_idx = idx;
	x->depth = depth;
	x->parent = p;
	if (p)
		p->n_kids++;
	return x;
}

static
void
node_free(
	struct n64texconv_palctx *ctx
)
{
	oct_node p;
	while (ctx->pool)
	{
		p = ctx->pool->parent;
		ctx->free(ctx->pool);
		ctx->pool = p;
	}
}

static
oct_node
node_insert(
	struct n64texconv_palctx *ctx
	, oct_node root
	, unsigned char *pix
)
{
	unsigned char i, bit, depth = 0;

	for (bit = 1 << 7; ++depth < 8; bit >>= 1) {
		i = !!(pix[1] & bit) * 4 + !!(pix[0] & bit) * 2 + !!(pix[2] & bit);
		if (!root->kids[i])
			root->kids[i] = node_new(ctx, i, depth, root);

		root = root->kids[i];
	}

	root->r += pix[0];
	root->g += pix[1];
	root->b += pix[2];
	root->count++;
	return root;
}

static
oct_node
node_fold(oct_node p)
{
	if (p->n_kids) abort();
	oct_node q = p->parent;
	q->count += p->count;

	q->r += p->r;
	q->g += p->g;
	q->b += p->b;
	q->n_kids --;
	q->kids[p->kid_idx] = 0;
	return q;
}

static
void
color_replace(oct_node root, unsigned char *pix)
{
	unsigned char i, bit;

	for (bit = 1 << 7; bit; bit >>= 1)
	{
		i = !!(pix[1] & bit) * 4 + !!(pix[0] & bit) * 2 + !!(pix[2] & bit);
		if (!root->kids[i])
			break;
		root = root->kids[i];
	}
	
	pix[0] = root->r;
	pix[1] = root->g;
	pix[2] = root->b;
}

#if 0
static
void
list_colors(oct_node root)
{
	int idx;
	
	for (idx = 0; 
	unsigned char i, bit;
	
	for (bit = 1 << 7; bit; bit >>= 1)
	{
		i = !!(pix[1] & bit) * 4 + !!(pix[0] & bit) * 2 + !!(pix[2] & bit);
		if (!root->kids[i])
			break;
		root = root->kids[i];
	}

	pix[0] = root->r;
	pix[1] = root->g;
	pix[2] = root->b;
}
#endif

static
void
error_diffuse(
	struct n64texconv_palctx *ctx
	, unsigned char *srcdst
	, int w
	, int h
	, node_heap *hp
)
{
	/* TODO alpha channel */
	oct_node nearest_color(int *v) {
		int i;
		int diff, max = 100000000;
		oct_node o = 0;
		for (i = 1; i < hp->n; i++) {
			diff =	  3 * abs(hp->buf[i]->r - v[0])
				+ 5 * abs(hp->buf[i]->g - v[1])
				+ 2 * abs(hp->buf[i]->b - v[2]);
			if (diff < max) {
				max = diff;
				o = hp->buf[i];
			}
		}
		return o;
	}

#	define POS(i, j) (3 * ((i) * w + (j)))
	int i, j;
	int *npx = ctx->calloc(sizeof(int), h * w * 3), *px;
	int v[3];
	unsigned char *pix = srcdst;
	oct_node nd;

#define C10 7
#define C01 5
#define C11 2
#define C00 1 
#define CTOTAL (C00 + C11 + C10 + C01)

	for (px = npx, i = 0; i < h; i++) {
		for (j = 0; j < w; j++, pix += 3, px += 3) {
			px[0] = (int)pix[0] * CTOTAL;
			px[1] = (int)pix[1] * CTOTAL;
			px[2] = (int)pix[2] * CTOTAL;
		}
	}
#define clamp(x, i) if (x[i] > 255) x[i] = 255; if (x[i] < 0) x[i] = 0
	pix = srcdst;
	for (px = npx, i = 0; i < h; i++) {
		for (j = 0; j < w; j++, pix += 3, px += 3) {
			px[0] /= CTOTAL;
			px[1] /= CTOTAL;
			px[2] /= CTOTAL;
			clamp(px, 0); clamp(px, 1); clamp(px, 2);

			nd = nearest_color(px);

			v[0] = px[0] - nd->r;
			v[1] = px[1] - nd->g;
			v[2] = px[2] - nd->b;

			pix[0] = nd->r; pix[1] = nd->g; pix[2] = nd->b;
			if (j < w - 1) {
				npx[POS(i, j+1) + 0] += v[0] * C10;
				npx[POS(i, j+1) + 1] += v[1] * C10;
				npx[POS(i, j+1) + 2] += v[2] * C10;
			}
			if (i >= h - 1) continue;

			npx[POS(i+1, j) + 0] += v[0] * C01;
			npx[POS(i+1, j) + 1] += v[1] * C01;
			npx[POS(i+1, j) + 2] += v[2] * C01;

			if (j < w - 1) {
				npx[POS(i+1, j+1) + 0] += v[0] * C11;
				npx[POS(i+1, j+1) + 1] += v[1] * C11;
				npx[POS(i+1, j+1) + 2] += v[2] * C11;
			}
			if (j) {
				npx[POS(i+1, j-1) + 0] += v[0] * C00;
				npx[POS(i+1, j-1) + 1] += v[1] * C00;
				npx[POS(i+1, j-1) + 2] += v[2] * C00;
			}
		}
	}
	ctx->free(npx);
}

static
void
palette_apply(
	struct n64texconv_palctx *ctx
	, void *pix
	, int w
	, int h
	, int dither
)
{
	assert(ctx);
	assert(pix);
	assert(w > 0);
	assert(h > 0);

	if (dither)
		error_diffuse(ctx, pix, w, h, &ctx->heap);
	
	else
	{
		unsigned char *pix8 = pix;
		unsigned int i;
		
		for (i = 0; i < w * h; i++, pix8 += 4)
			color_replace(ctx->root, pix8);
	}
}


/* endian reverse bytes */
static
void
reverse_u32(unsigned char *list, unsigned int num_u32)
{
	while (num_u32--)
	{
		unsigned char t[4] = {list[3], list[2], list[1], list[0]};
		list[0] = t[0];
		list[1] = t[1];
		list[2] = t[2];
		list[3] = t[3];
		list += 4;
	}
}


/* quantize a single image */
static
void
color_quant(
	void *srcdst
	, int w
	, int h
	, int n_colors
	, int dither
	, void *calloc(size_t, size_t)
	, void *realloc(void *, size_t)
	, void free(void *)
)
{
	struct n64texconv_palctx *ctx;
	
	/* thanks, endianness... */
	reverse_u32(srcdst, w * h);
	
	ctx = n64texconv_palette_new(n_colors, 0, calloc, realloc, free);
	n64texconv_palette_queue(ctx, srcdst, w, h, 0);
	n64texconv_palette_exec(ctx);
	n64texconv_palette_free(ctx);
	
	/* thanks, endianness... */
	reverse_u32(srcdst, w * h);
}

/* returns number of palette colors */
static
int
palette_generate(struct n64texconv_palctx *ctx)
{
	assert(ctx);
	
	while (ctx->heap.n > ctx->n_colors + 1)
		heap_add(
			ctx->realloc
			, &ctx->heap
			, node_fold(pop_heap(&ctx->heap))
		);
	
	oct_node got;
	double c;
	unsigned int i;
	for (i = 1; i < ctx->heap.n; i++) {
		got = ctx->heap.buf[i];
		c = got->count;
		got->r = got->r / c + .5;
		got->g = got->g / c + .5;
		got->b = got->b / c + .5;
	}
	
	/* apply palette to every queued image, and construct color list */
	struct pqueue *queue;
	struct pqueue *next;
	int n_colors = 0;
	uint32_t *color = ctx->colors;
	for (n_colors = 0, queue = ctx->queue; queue; queue = next)
	{
		/* apply palette to every queued image */
		palette_apply(ctx, queue->pix, queue->w, queue->h, queue->dither);
		next = queue->next;
		
		/* propagate color list with colors found */
		if (color)
		{
			uint32_t *image = queue->pix;
			unsigned int i;
			int c;
			
			/* for every pixel in image */
			for (i = 0; i < queue->w * queue->h; ++i)
			{
				/* test if pixel color is in palette */
				for (c = 0; c < n_colors; ++c)
					if (color[c] == image[i])
						break;
				
				/* pixel color is not in palette */
				if (c == n_colors)
				{
//					fprintf(stderr, "push color %08X (%d)\n", image[i], c);
					
					/* this should never happen */
					assert(n_colors < (ctx->n_colors + ctx->n_alpha));
					
					/* add pixel color to palette */
					color[c] = image[i];
					n_colors += 1;
				}
			}
		}
		
		/* free queued list item when done */
		ctx->free(queue);
	}
	
	return n_colors;
}


struct n64texconv_palctx *
n64texconv_palette_new(
	int colors
	, void *dst
	, void *calloc(size_t, size_t)
	, void *realloc(void *, size_t)
	, void free(void *)
)
{
	if (colors < 1)
		colors = 1;
	if (colors > 256)
		colors = 256;
	
	assert(calloc);
	assert(realloc);
	assert(free);
	
	struct n64texconv_palctx *ctx = calloc(1, sizeof(*ctx));
	ctx->calloc = calloc;
	ctx->realloc = realloc;
	ctx->free = free;
	
	ctx->root = node_new(ctx, 0, 0, 0);
	ctx->n_colors = colors;
	ctx->colors = dst;
	
	return ctx;
}

void
n64texconv_palette_queue(
	struct n64texconv_palctx *ctx
	, void *pix
	, int w
	, int h
	, int dither
)
{
	assert(ctx);
	assert(pix);
	assert(w > 0);
	assert(h > 0);
	
	/* add to queue */
	struct pqueue *next = ctx->calloc(1, sizeof(*next));
	next->next = ctx->queue;
	next->pix = pix;
	next->w = w;
	next->h = h;
	next->dither = dither;
	ctx->queue = next;
	
	unsigned char *pix8 = pix;
	unsigned int i;
	
	for (i = 0; i < w * h; i++, pix8 += 4)
		heap_add(
			ctx->realloc
			, &ctx->heap
			, node_insert(
				ctx
				, ctx->root
				, pix8
			)
		);
}


/* returns number of colors in palette */
int
n64texconv_palette_exec(struct n64texconv_palctx *ctx)
{
	assert(ctx);
	
	int palcolors = 0;
	
	if (ctx->queue)
		palcolors = palette_generate(ctx);
	
	return palcolors + ctx->n_alpha;
}


/* make room for alpha colors in palette */
void
n64texconv_palette_alpha(struct n64texconv_palctx *ctx, int alpha)
{
	assert(ctx);
	
	ctx->n_alpha = alpha;
	ctx->n_colors -= alpha;
}


void
n64texconv_palette_free(struct n64texconv_palctx *ctx)
{
	assert(ctx);
	
	node_free(ctx);
	ctx->free(ctx->heap.buf);
	
	ctx->free(ctx);
}

/* end colorquantize.c */

#define ACFUNC_ARGS                  \
	void *rgba8888                    \
	, int w                           \
	, int h                           \
	, int max_alpha_colors            \
	, void *calloc(size_t, size_t)    \
	, void *realloc(void *, size_t)   \
	, void free(void *)


/* replace all invisible pixels with color (includes alpha channel) */
static
void
acfunc__invisibles(void *rgba8888, unsigned int num, uint32_t color)
{
	uint32_t *pix = rgba8888;
	
	while (num--)
	{
		if (!(pix[0] & 0x000000FF))
			*pix = color;
		++pix;
	}
}


static
int
acfunc_user(ACFUNC_ARGS)
{
	/* this function should never be called; return err code */
	assert(0 && "this function should never be called");
	return -1;
}


static
int
acfunc_black(ACFUNC_ARGS)
{
	/* replace all invisible pixels with black */
	acfunc__invisibles(rgba8888, w * h, 0);
	
	/* there is now 1 unique alpha color */
	return 1;
}


static
int
acfunc_white(ACFUNC_ARGS)
{
	/* replace all invisible pixels with white */
	acfunc__invisibles(rgba8888, w * h, 0xFFFFFF00);
	
	/* there is now 1 unique alpha color */
	return 1;
}

/* https://github.com/ruozhichen/rgb2Lab-rgb2hsl/blob/master/LAB.py */
static
double *
rgb2lab(unsigned char rgb[3])
{
	static double LAB[3];
	double RGB[3];
	double XYZ[3];
	double L = 0;
	int i;
	
	for (i = 0; i < 3; ++i)
	{
		RGB[i] = rgb[i];
		RGB[i] /= 255.0f;
	}
	
	XYZ[0] = RGB[0]*0.4124+RGB[1]*0.3576+RGB[2]*0.1805;
	XYZ[1] = RGB[0]*0.2126+RGB[1]*0.7152+RGB[2]*0.0722;
	XYZ[2] = RGB[0]*0.0193+RGB[1]*0.1192+RGB[2]*0.9505;
	
	XYZ[0]/=95.045/100.0;
	XYZ[1]/=100.0/100.0;
	XYZ[2]/=108.875/100.0;
	
	for (i = 0; i < 3; ++i)
	{
		double v = XYZ[i];
		
		if (v > 0.008856)
		{
			v = pow(v, 1.0/3.0);
			if (i == 1)
				L = 116.0 * v - 16.0;
		}
		else
		{
			v *= 7.787;
			v += 16.0 / 116.0;
			if (i == 1)
				L = 903.3 * XYZ[i];
		}
		
		XYZ[i] = v;
	}
	
	double a = 500.0 * (XYZ[0] - XYZ[1]);
	double b = 200.0 * (XYZ[1] - XYZ[2]);
	
	LAB[0] = L;
	LAB[1] = a;
	LAB[2] = b;
	
	return LAB;
}

/* https://github.com/ruozhichen/rgb2Lab-rgb2hsl/blob/master/LAB.py */
static
unsigned char *
lab2rgb(double lab[3])
{
	static unsigned char rgb[3];
	double L = lab[0];
	double a = lab[1];
	double b = lab[2];
	//double d = 6.0 / 29.0;
	
	double T1 = 0.008856;
	double T2 = 0.206893;
	double d = T2;
	double fy = pow((L + 16.0) / 116.0, 3.0);
	double fx = fy + a / 500.0;
	double fz = fy - b / 200.0;
	if (fy <= T1) fy = L / 903.3;
	double Y = fy;
	double X;
	double Z;
	int i;
	
	/* calculate XYZ[1], XYZ[0]=a/500.0+XYZ[1] */
	if (fy > T1)
		fy = pow(fy, 1.0/3.0);
	else
		fy = 7.787*fy+16.0/116.0;
	
	/* compute original XYZ[0] */
	/* v^3>T1, so v>T1^(1/3)= */
	fx = fy + a / 500.0;
	if (fx > T2)
		X = pow(fx, 3.0);
	else
		X = (fx-16.0/116.0)/7.787;
	
	/* compute original XYZ[2] */
	fz = fy - b / 200.0;
	if (fz > T2)
		Z = pow(fz, 3.0);
	else
		Z = (fz - 16.0 / 116.0) / 7.787;
	
	X*=0.95045;
	Z*=1.08875;
	double R = 3.240479 * X + (-1.537150) * Y + (-0.498535) * Z;
	double G = (-0.969256) * X + 1.875992 * Y + 0.041556 * Z;
	double B = 0.055648 * X + (-0.204043) * Y + 1.057311 * Z;
	double RGB[] = {R, G, B};
	for (i = 0; i < 3; ++i)
	{
		int t = fmin(round(RGB[i] * 255), 255);
		rgb[i] = fmax(t, 0);
	}
	
	return rgb;
}


static
int
acfunc_average(ACFUNC_ARGS)
{
	unsigned char *pix = rgba8888;
	unsigned int i;
	unsigned int found = 0;
	double lab[3] = {0};
	uint32_t alpha = 0;
	
	/* derive average color of visible pixels */
	for (i = 0; i < w * h; ++i, pix += 4)
	{
		/* XXX endianness */
		unsigned char p[] = {pix[3], pix[2], pix[1], pix[0]};
		
		/* skip invisible pixels */
		if (!(p[3]&0x80))
			continue;
		
		/* add color to average */
		double *conv = rgb2lab(p);
		lab[0] += conv[0];
		lab[1] += conv[1];
		lab[2] += conv[2];
		found += 1;
	}
	if (found)
	{
		lab[0] /= found;
		lab[1] /= found;
		lab[2] /= found;
		
		unsigned char *rgb = lab2rgb(lab);
		alpha |= ((int)rgb[0]) << 24;
		alpha |= ((int)rgb[1]) << 16;
		alpha |= ((int)rgb[2]) <<  8;
	}
	
	/* replace all invisible pixels with new color */
	acfunc__invisibles(rgba8888, w * h, alpha);
	
	/* there is now 1 unique alpha color */
	return 1;
}

/* 
 * if (max_alpha_colors == 0), the indexing steps are skipped
 */
static
int
acfunc_edge_expand(ACFUNC_ARGS)
{
#define PINV(X) (!(X & 0x000000FF))
	uint32_t     *pix = rgba8888;
	uint32_t     *EDpix;         /* edge-detected image */
	uint32_t     *EDcolor;       /* color list containing only solids */
	unsigned int  n_EDcolor = 0; /* num pixels in EDcolor */
	
	/* alloc edge-detected image */
	if (max_alpha_colors == 0)
	{
		/* color indexing is disabled, so skip that step */
		EDpix = realloc(0, w * h * 4);
		memcpy(EDpix, pix, w * h * 4);
		goto L_skip_indexing;
	}
	else
		EDpix = calloc(1, w * h * 4);
	
	/* first pass: construct edge-detected image */
	/* loop through every row */
	for (int y = 0; y < h; ++y)
	{
		/* loop through every column */
		for (int x = 0; x < w; ++x)
		{
			uint32_t   pC  =    pix[y * w + x];
			uint32_t *edC  = &EDpix[y * w + x];
			
			/* skip invisible pixels */
			if (PINV(pC))
				continue;
			
			/* sample pixels above below left and right of current */
			int yup    = imx(y - 1,     0);
			int ydown  = imn(y + 1, h - 1);
			int xleft  = imx(x - 1,     0);
			int xright = imn(x + 1, w - 1);
			uint32_t  pUP     = pix[ yup   * w + x      ];
			uint32_t  pDOWN   = pix[ ydown * w + x      ];
			uint32_t  pLEFT   = pix[ y     * w + xleft  ];
			uint32_t  pRIGHT  = pix[ y     * w + xright ];
			
			/* if any sampled pixel is invisible, xfer to edge map */
			if (PINV(pUP) || PINV(pDOWN) || PINV(pLEFT) || PINV(pRIGHT))
			{
				*edC = pC;
				n_EDcolor += 1;
			}
		}
	}
	
	/* allocate list of colors in edge-detected image */
	EDcolor = realloc(0, n_EDcolor * 4);
	
	/* second pass: propagate color list */
	n_EDcolor = 0;
	/* loop through every row */
	for (int y = 0; y < h; ++y)
	{
		/* loop through every column */
		for (int x = 0; x < w; ++x)
		{
			uint32_t c = EDpix[y * w + x];
			
			/* skip invisible pixels */
			if (PINV(c))
				continue;
			
			/* all visible pixels get added to the color map */
			EDcolor[n_EDcolor] = c;
			n_EDcolor += 1;
		}
	}
	
	/* quantize color list */
	color_quant(
		EDcolor
		, n_EDcolor
		, 1
		, max_alpha_colors
		, 0
		, calloc
		, realloc
		, free
	);
	
	/* third pass: xfer color list colors back to edge-detected image */
	n_EDcolor = 0;
	/* loop through every row */
	for (int y = 0; y < h; ++y)
	{
		/* loop through every column */
		for (int x = 0; x < w; ++x)
		{
			uint32_t *c = &EDpix[y * w + x];
			
			/* skip invisible pixels */
			if (PINV(*c))
				continue;
			
			/* all visible pixels retrieve new value from color list */
			*c = EDcolor[n_EDcolor];
			n_EDcolor += 1;
		}
	}
	free(EDcolor);
	
L_skip_indexing: do{}while(0);
	/* at this point, `EDpix` contains a color-reduced     *
	 * edge-detected copy of the input rgba8888 pixel data */
	/* if color-indexing is disabled, it will contain an   *
	 * exact copy of the input pixel data `rgba8888`       */
	
	/* expand every pixel */
	int has_invis = 1;
	while (has_invis)
	{
		has_invis = 0;
		/* loop through every row */
		for (int y = 0; y < h; ++y)
		{
			/* loop through every column */
			for (int x = 0; x < w; ++x)
			{
				uint32_t   pC  =  EDpix[y * w + x];
				
				/* skip invisible pixels, and pixels just processed */
				if (PINV(pC) || ((pC & 0xFF) == 0x01))
					continue;
				
				/* sample pixels above below left and right of current */
				int yup    = imx(y - 1,     0);
				int ydown  = imn(y + 1, h - 1);
				int xleft  = imx(x - 1,     0);
				int xright = imn(x + 1, w - 1);
				uint32_t  *pUP     = &EDpix[ yup   * w + x      ];
				uint32_t  *pDOWN   = &EDpix[ ydown * w + x      ];
				uint32_t  *pLEFT   = &EDpix[ y     * w + xleft  ];
				uint32_t  *pRIGHT  = &EDpix[ y     * w + xright ];
				
				/* if any sampled pixel is invisible, edit *
				 * color and mark as just processed        */
				#define PXPROC(P)           \
					if (PINV(*P)) {          \
						*P = pC & 0xFFFFFF01; \
					}
				PXPROC(pUP)
				PXPROC(pDOWN)
				PXPROC(pLEFT)
				PXPROC(pRIGHT)
				#undef PXPROC
			}
		}
		
		/* stepped through image once; mark all processed pixels *
		 * as regular opaque pixels for the next iteration       */
		unsigned int i;
		for (i = 0; i < w * h; ++i)
		{
			if (EDpix[i] & 0xFF)
				EDpix[i] |= 0xFF;
			else
				has_invis = 1;
		}
	}
	
#if 0
	/* this test is to see result quickly */
	memcpy(pix, EDpix, w * h * 4);
#endif
	
	/* transfer alpha colors to original image */
	for (unsigned int i = 0; i < w * h; ++i)
		if (PINV(pix[i]))
			/* 0xFFFFFFFF can be used to test png result */
			pix[i] = EDpix[i] & 0xFFFFFF00;
	
	/* cleanup */
	free(EDpix);
	/* don't worry, EDcolor was freed earlier */
	
	/* num unique alpha colors in image */
	return max_alpha_colors;
#undef PINV
}


int
n64texconv_acgen(
	void *rgba8888
	, int w
	, int h
	, enum n64texconv_acgen formula
	, int max_alpha_colors
	, void *calloc(size_t, size_t)
	, void *realloc(void *, size_t)
	, void free(void *)
)
{
	assert(rgba8888);
	assert(w > 0);
	assert(h > 0);
	assert(realloc);
	assert(free);
	assert(formula >= 0 && formula < N64TEXCONV_ACGEN_MAX);
	assert(max_alpha_colors >= 0);
	
	int Nacolor = 0;
	uint32_t acolor[256] = {0};
	uint32_t *pix = rgba8888;
	unsigned int i;
	unsigned int num_opa = 0;
	unsigned int num_invis = 0;
	
	/* converter array */
	int
	(*acfunc[])(ACFUNC_ARGS) = {
		[N64TEXCONV_ACGEN_EDGEXPAND] = acfunc_edge_expand
		, [N64TEXCONV_ACGEN_AVERAGE] = acfunc_average
		, [N64TEXCONV_ACGEN_BLACK  ] = acfunc_black
		, [N64TEXCONV_ACGEN_WHITE  ] = acfunc_white
		, [N64TEXCONV_ACGEN_USER   ] = acfunc_user
	};
	
	if (max_alpha_colors > 8)
		max_alpha_colors = 8;
	
	for (i = 0; i < w * h; ++i)
	{
		int c;
		
		/* skip visible pixels */
		if (pix[i] & 0x000000FF)
		{
			num_opa += 1;
			continue;
		}
		else
			num_invis += 1;
		
		/* skip checking color list if already exceeded */
		if (Nacolor >= max_alpha_colors)
			continue;
		
		/* now check color against every unique invisible color */
		for (c = 0; c < Nacolor; ++c)
			if (pix[i] == acolor[c])
				break;
		
		/* pixel does not exist in palette */
		if (c == Nacolor)
		{
			/* palette exceeds max allowable alpha colors */
			if (Nacolor >= max_alpha_colors)
				break;
			
			/* add it to palette */
			acolor[c] = pix[i];
			Nacolor += 1;
		}
	}
	
	/* no invisible pixels were found */
	if (!num_invis)
		return 0;
	
	/* every pixel is invisible, so make entire image black */
	if (!num_opa)
	{
		return acfunc[N64TEXCONV_ACGEN_BLACK](
			rgba8888, w, h, max_alpha_colors, calloc, realloc, free
		);
	}
	
//	fprintf(stderr, "%d invisible pixel colors found\n", Nacolor);
	
	/* the user wants to use invisible pixel colors already present */
	if (formula == N64TEXCONV_ACGEN_USER)
	{
		/* the number of unique invisible pixel colors is small enough *
		 * that major quality reduction won't happen leaving it as-is  */
		if (!max_alpha_colors || Nacolor < max_alpha_colors)
			return Nacolor;
		
		/* otherwise, let's fall back to edge-expand formula */
		else
			formula = N64TEXCONV_ACGEN_EDGEXPAND;
	}
	
	/* doing edge expansion with only one color? may as well average */
	if (formula == N64TEXCONV_ACGEN_EDGEXPAND	&& max_alpha_colors == 1)
		formula = N64TEXCONV_ACGEN_AVERAGE;
	
#if 0 /* not anymore I think */
	/* edge-expand formula requires an image to be at least 3x3 */
	if (formula == N64TEXCONV_ACGEN_EDGEXPAND && (w < 3 || h < 3))
		/* fall back to averaging formula if requirements not met */
		formula = N64TEXCONV_ACGEN_AVERAGE;
#endif
	
	return acfunc[formula](
		rgba8888, w, h, max_alpha_colors, calloc, realloc, free
	);
}

