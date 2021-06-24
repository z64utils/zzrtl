#ifndef ZZRTL_H_INCLUDED
#define ZZRTL_H_INCLUDED

#include <stddef.h>

#include "n64texconv.h"   /* for texture format conversions */
#include "find_bytes.h"   /* for pattern searching */

/* types */
struct rom;
struct folder;
struct conf;
struct fldr_item;

/* setters */
void die_no_waiting(int x);
void zzrtl_use_cache(int on);

void
zzrtl_waitkey(void);

/****
 * generic functions
 ****/

/* check if file has extension */
char *
is_ext(char *fn, char *ext);

/* safe calloc */
void *
calloc_safe(size_t nmemb, size_t size);

/* safe malloc */
void *
malloc_safe(size_t size);

/* safe realloc */
void *
realloc_safe(void *ptr, size_t size);

/* terminate with a custom error message */
void
die(char *fmt, ...);

/* load a file */
void *
loadfile(char *fn, int *sz, int optional);

/* extract tsv string */
char *
tsv_col_row(char *tsv, char *colname, int row);

/* get 32-bit value from raw data */
int
get32(void *_data);

/* get 24-bit value from raw data */
int
get24(void *_data);

/* get 16-bit value from raw data */
int
get16(void *_data);

/* get 32-bit value from raw data */
int
get8(void *_data);

/* put 32-bit value into raw data */
void
put32(void *_data, int v);

/* put 24-bit value into raw data */
void
put24(void *_data, int v);

/* put 16-bit value into raw data */
void
put16(void *_data, int v);

/* put 8-bit value into raw data */
void
put8(void *_data, int v);

/* get unsigned equivalent of signed char */
int
u8(char c);

/* get signed char equivalent of int */
int
s8(int c);

/* cast int to unsigned short */
int
u16(int c);

/* cast int to signed short */
int
s16(int c);

/* cast a and b to u32 and do operation */
unsigned int
u32op(unsigned int a, char *op, unsigned int b);

/* get string at index `idx` in string `*list`; *
 * returns 0 if anything goes awry;             *
 * used for fetching name from name list        */
char *
substring(char *list, int index);

/* get vram size of overlay */
unsigned int
ovl_vram_sz(void *_ovl, int sz);

/* load png */
void *
load_png(char *fn, int *w, int *h);

/****
 * directory functions
 ****/

/* returns 1 if directory of given name exists */
int
dir_exists(char *dir);

/* enter a directory (folder)
   NOTE: directory is created if it doesn't exist */
void
dir_enter(char *dir);

/* enter the directory of a provided file */
void
dir_enter_file(char *fn);

/* leave last-entered directory */
void
dir_leave(void);

/* make a compliant directory name */
char *
dir_mkname(int idx, char *str);

/* set style used by dir_mkname
   valid options:
      "pre"   : uses form "%d - %s"
      "preX"  : uses form "0x%X - %s"
      "post"  : uses form "%s (%d)"
      "postX" : uses form "%s (0x%X)"
*/
void
dir_use_style(char *style);

/* returns non-zero if file of given name exists, 0 otherwise */
int
file_exists(char *fn);

/****
 * rom functions
 ****/

/* allocate a rom structure and load rom file */
struct rom *
rom_new(struct rom *rom, char *fn);

/* free a rom structure */
void
rom_free(struct rom *rom);

/* set flag in rom structure */
void
rom_flag(struct rom *rom, const char *id, int on);

/* get pointer to raw data at offset `ofs` in rom */
void *
rom_raw(struct rom *rom, unsigned int ofs);

/* save rom to disk using specified filename */
void
rom_save(struct rom *rom, char *fn);

/* set rom alignment */
void
rom_align(struct rom *rom, int alignment);

/* compress rom using specified algorithm */
/* valid options: "yaz" */
void
rom_compress(struct rom *rom, char *enc, int mb);

/* seek offset within rom */
void
rom_seek(struct rom *rom, unsigned int offset);

/* seek offset within rom, relative to current offset */
void
rom_seek_cur(struct rom *rom, int offset);

/* get offset within rom */
unsigned int
rom_tell(struct rom *rom);

/* get size of rom */
unsigned int
rom_size(struct rom *rom);

/* enable injection prefix */
void *
rom_inject_prefix(struct rom *rom);

/* use special injection mode */
void
rom_inject_mode(struct rom *rom, char *mode);

/* inject file into rom */
void *
rom_inject(struct rom *rom, char *fn, int comp);

/* loads PNG, converts to N64 format, and injects into rom */
void *
rom_inject_png(
	struct rom *rom
	, char *fn
	, enum n64texconv_fmt fmt
	, enum n64texconv_bpp bpp
	, int comp
);

/* inject raw data into rom, returning a pointer to the result;
   it also propagates start and end offsets with result, or 0;
   if a pointer to 0 or a size of 0 is specified, it returns 0 */
void *
rom_inject_raw(struct rom *rom, void *raw, unsigned int sz, int comp);

/* inject data over existing dma entry (file size must match) */
void *
rom_inject_raw_dma(struct rom *rom, void *raw, unsigned int sz, int comp, int idx);

/* inject file over existing dma entry (file size must match) */
void *
rom_inject_dma(struct rom *rom, char *fn, int comp, int idx);

/* get start offset of file injected with inject() */
/* NOTE: will be 0 if inject() failed */
unsigned int
rom_file_start(struct rom *rom);

/* get end offset of file injected with inject() */
/* NOTE: will be 0 if inject() failed */
unsigned int
rom_file_end(struct rom *rom);

/* get pointer to data of most recently written
   file; will be 0 if inject() failed */
void *
rom_file(struct rom *rom);

/* get size of file injected with inject() */
/* NOTE: will be 0 if inject() failed */
unsigned int
rom_file_sz(struct rom *rom);

/* get dma index of most recently written file (-1 if none) */
int
rom_file_dma(struct rom *rom);

/* extract raw data to a file */
void
rom_extract(struct rom *rom, char *fn, unsigned start, unsigned end);

/* extract raw data to a file, based on dma entry */
void
rom_extract_dma(struct rom *rom, char *fn, int idx);

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
);

/* write and advance `num` bytes within rom */
void
rom_write(struct rom *rom, void *data, int sz);

/* write and advance 4 bytes within rom */
void
rom_write32(struct rom *rom, unsigned int value);

/* write and advance 3 bytes within rom */
void
rom_write24(struct rom *rom, int value);

/* write and advance 2 bytes within rom */
void
rom_write16(struct rom *rom, int value);

/* write and advance 1 byte within rom */
void
rom_write8(struct rom *rom, int value);

/* read and advance 4 bytes within rom */
int
rom_read32(struct rom *rom);

/* read and advance 3 bytes within rom */
int
rom_read24(struct rom *rom);

/* read and advance 2 bytes within rom */
int
rom_read16(struct rom *rom);

/* read and advance 1 byte within rom */
int
rom_read8(struct rom *rom);

/* get filename of rom */
char *
rom_filename(struct rom *rom);

/* specify start of dmadata, and number of entries */
void
rom_dma(struct rom *rom, unsigned int offset, int num_entries);

/* register offset in rom, at which to write dma index (u16) */
void
rom_dma_register(struct rom *rom, unsigned int offset);

/* grab injection prefix */
void *
rom_dma_prefix(struct rom *rom);

/* get offset of file by dma index */
unsigned int
rom_dma_offset(struct rom *rom, int idx);

/* call this function when you're finished with the dma stuff;
   must be called before you inject data */
void
rom_dma_ready(struct rom *rom);

/* mark one dma entry (by index) as overwritable */
void
rom_dma_queue_one(struct rom *rom, int idx);

/* apply dma_queue_one() to indices from start to
   end, inclusive (aka start <= idx <= end) */
void
rom_dma_queue(struct rom *rom, int start, int end);

/* set compression flag on one dma entry (by index);
   if (comp == 1), file is marked for compression */
void
rom_dma_compress_one(struct rom *rom, int idx, int comp);

/* apply rom_dma_compress() to indices from start to
   end, inclusive (aka start <= idx <= end) */
void
rom_dma_compress(struct rom *rom, int start, int end, int comp);

/* reencode one existing archive within rom, by index */
void
rom_dma_rearchive_one(
	struct rom *rom
	, int idx
	, char *from
	, char *to
	, int relocate
);

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
);

/* apply cloudpatch to rom */
void
rom_cloudpatch(struct rom *rom, int ofs, char *fn);


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
folder_new(struct folder *folder, char *ext);

/* free a folder structure */
void
folder_free(struct folder *folder);

/* get current folder name */
char *
folder_name(struct folder *folder);

/* get numerical part of current folder name */
int
folder_index(struct folder *folder);

/* go to next folder in list; returns 0 when there is no next folder */
int
folder_next(struct folder *folder);

/* get number of items in folder list */
int
folder_count(struct folder *folder);

/* get number of items between current and end */
int
folder_remaining(struct folder *folder);

/* get highest index detected in list */
int
folder_max(struct folder *folder);

/* sort struct fldr_item array by ascending index */
int
fldr_item_ascending(const void *_a, const void *_b);

/* locate folder item by name */
struct fldr_item *
folder_find_name(struct folder *folder, char *name);

/* locate folder item by name, ignoring extension */
struct fldr_item *
folder_find_name_n_ext(struct folder *folder, char *name);


/****
 * conf functions
 ****/

/* allocate a conf structure */
struct conf *
conf_new(struct conf *conf, char *fn, char *format);

/* free a conf structure */
void
conf_free(struct conf *conf);

/* get integer value associated with name */
int
conf_get_int(struct conf *conf, char *name);

/* get pointer to string associated with name */
char *
conf_get_str(struct conf *conf, char *name);

/* r3 */
/* get name of selected list item (lists only) */
/* returns 0 at end of list */
char *
conf_name(struct conf *conf);

/* r3 */
/* get value of selected list item, as string (lists only) */
/* returns 0 at end of list */
char *
conf_value(struct conf *conf);

/* in the case of a table, go to next line;
   returns 0 when there is no next line */
int
conf_next(struct conf *conf);

/* in the case of a table, this will be non-zero if there are
   rows remaining, and 0 otherwise */
int
conf_remaining(struct conf *conf);

/* returns non-zero if a name can be found inside a conf structure */
int
conf_exists(struct conf *conf, char *fn);


#endif /* ZZRTL_H_INCLUDED */

