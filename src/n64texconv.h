#ifndef N64TEXCONV_H_INCLUDED
#define N64TEXCONV_H_INCLUDED

#include <stdlib.h> /* size_t */

struct n64texconv_palctx;

enum n64texconv_fmt
{
	N64TEXCONV_RGBA = 0
	, N64TEXCONV_YUV
	, N64TEXCONV_CI
	, N64TEXCONV_IA
	, N64TEXCONV_I
	, N64TEXCONV_1BIT
	, N64TEXCONV_FMT_MAX
};


enum n64texconv_bpp
{
	N64TEXCONV_4 = 0
	, N64TEXCONV_8
	, N64TEXCONV_16
	, N64TEXCONV_32
};

enum n64texconv_acgen
{
	                                 /* all invisible pixels become...*/
	N64TEXCONV_ACGEN_EDGEXPAND = 0   /* edge expand algorithm         */
	, N64TEXCONV_ACGEN_AVERAGE       /* avg color of visible pixels   */
	, N64TEXCONV_ACGEN_WHITE         /* white (FFFFFF00)              */
	, N64TEXCONV_ACGEN_BLACK         /* black (00000000)              */
	, N64TEXCONV_ACGEN_USER          /* use colors already in image   *
	                                  * (2many = fallback 2 edgXpand) */
	, N64TEXCONV_ACGEN_MAX           /* num items in this enum list   */
};


/* convert N64 texture data to RGBA8888
 * returns 0 (NULL) on success, pointer to error string otherwise
 * error string will be returned if...
 * * invalid fmt/bpp combination is provided
 * * color-indexed format is provided, yet `pal` is 0 (NULL)
 * `pal` can be 0 (NULL) if format is not color-indexed
 * `dst` must point to data you have already allocated
 * `pix` must point to pixel data
 * `w` and `h` must > 0
 * NOTE: `dst` and `pix` can be the same to convert in-place, but
 *       you must ensure the buffer is large enough for the result
 */
const char *
n64texconv_to_rgba8888(
	unsigned char *dst
	, unsigned char *pix
	, unsigned char *pal
	, enum n64texconv_fmt fmt
	, enum n64texconv_bpp bpp
	, int w
	, int h
);


/* convert RGBA8888 to N64 texture data
 * returns 0 (NULL) on success, pointer to error string otherwise
 * error string will be returned if...
 * * invalid fmt/bpp combination is provided
 * * color-indexed format is provided (conversion to ci is unsupported)
 * `dst` must point to data you have already allocated
 * `pix` must point to pixel data
 * `w` and `h` must be > 0
 * NOTE: `dst` and `pix` can be the same to convert in-place, but
 *       you must ensure the buffer is large enough for the result
 */
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
);


/* convert RGBA8888 to N64 texture data and back, reducing color depth
 * returns 0 on success, pointer to error string otherwise
 */
const char *
n64texconv_to_n64_and_back(
	unsigned char *pix
	, unsigned char *pal
	, int pal_colors
	, enum n64texconv_fmt fmt
	, enum n64texconv_bpp bpp
	, int w
	, int h
);


/* returns a pointer to a palette context */
/* `dst` is where the colors will go */
/* `colors` is the max colors for the palette */
struct n64texconv_palctx *
n64texconv_palette_new(
	int colors
	, void *dst
	, void *calloc(size_t, size_t)
	, void *realloc(void *, size_t)
	, void free(void *)
);


/* adds an rgba8888 image to a palette context's queue */
void
n64texconv_palette_queue(
	struct n64texconv_palctx *ctx
	, void *pix
	, int w
	, int h
	, int dither
);


/* make room for alpha colors in palette */
void
n64texconv_palette_alpha(struct n64texconv_palctx *ctx, int alpha);


/* quantizes all queued images, generates palette, and returns the
 * number of colors in the generated palette
 */
int
n64texconv_palette_exec(struct n64texconv_palctx *ctx);


/* frees context
 */
void
n64texconv_palette_free(struct n64texconv_palctx *ctx);


/* takes an image containing invisible pixels and generates new colors
 * for them, returning the number of unique invisible colors (<0 = err)
 * if (max_alpha_colors == 0), the indexing steps are skipped
 */
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
);

#endif /* N64TEXCONV_H_INCLUDED */

