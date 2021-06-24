#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "zzrtl.h"
#include "preproc.h"

#define FERR(x) { fprintf(stderr, x"\n"); exit(EXIT_FAILURE); }
	
static
struct rom *rom = 0;

static
char *Acodec = 0;

static int dma_num = 0;

static
void
compress(int start, int end)
{
	rom_dma_compress(rom, start, end, 1);
}

static
void
compress_no(int start, int end)
{
	rom_dma_compress(rom, start, end, 0);
}

static
void
rearchive(int start, int end)
{
//	rom_dma_rearchive(rom, start, end, "yaz", Acodec, 0);
	/* new default: relocating always enabled */
	rom_dma_rearchive(rom, start, end, "yaz", Acodec, 1);
}

static
void
rearchive_rl(int start, int end)
{
	rom_dma_rearchive(rom, start, end, "yaz", Acodec, 1);
}

static
void
which(char *str, void op(int start, int end))
{
	int len;
	int last_int = -1;
	int last_op = 0;
	int cur;
	while (*str)
	{
		/* get length of token at current position in string */
		len = preproc_toklen(str);
		
		/* is a number, or variable */
		if (isdigit(*str) || !memcmp(str, "END", 3))
		{
			/* END is shorthand for last entry */
			if (!memcmp(str, "END", 3))
				cur = dma_num;
			
			/* otherwise, it's a number */
			else
				sscanf(str, "%d", &cur);
			
			if (cur < last_int)
				FERR("which items must ascend in value");
			
			/* apply operations on item(s) */
			if (last_op == '-')
				op(last_int, cur);
			else
				op(cur, cur);
			
			/* prevents operating on this item again when
			   "through" is used */
			cur += 1;
		}
		
		/* tells us either "through" or "single item" */
		else if (*str == '-' || *str == ',')
		{
			if (last_int < 0)
				FERR("which params must start with number");
			last_op = *str;
		}
		
		/* unknown character encountered */
		else
		{
			fprintf(stderr, "unknown which operator '%c'\n", *str);
			exit(EXIT_FAILURE);
		}
		
		/* advance */
		str += len;
		last_int = cur;
	}
}

void
main_compress(char *argstring)
{
	char *str;
	char *ss;
	int len;
	
	char *Aif = 0;
	char *Aof = 0;
	char *Adma = 0;
	int mb = 0;
	int dma_start = 0;
	
	/* times through loop;
	   0 = 'if'
	   1 = '='
	   2 = 'in.z64'
	   3 = ' ' (then it resets to 0) */
	int times = 0;
	
	enum
	{
		NONE = 0
		, IF
		, OF
		, CODEC
		, DMA
		, REARCHIVE
		, REARCHIVErl
		, COMPRESS
		, COMPRESSno
		, MB
		, CLOUDPATCH
	} work = 0;
	
	argstring = strdup(argstring);
	if (!argstring)
		FERR("memory error\n");
	
	for (str = argstring; *str; str += len)
	{
		/* get length of token at current position in string */
		len = preproc_toklen(str);
		
		//fprintf(stderr, "'%.*s'\n", len, str);
		
		/* strip quotes if there are any */
		ss = str;
		preproc_strip_quotes(&ss, len);
		
		if (times == 0)
		{
			if (!memcmp(ss, "if", len))
				work = IF;
			else if (!memcmp(ss, "of", len))
				work = OF;
			else if (!memcmp(ss, "codec", len))
				work = CODEC;
			else if (!memcmp(ss, "dma", len))
				work = DMA;
			else if (!memcmp(ss, "mb", len))
				work = MB;
			else if (!memcmp(ss, "compress", len))
				work = COMPRESS;
			else if (!memcmp(ss, "NOcompress", len))
				work = COMPRESSno;
			else if (!memcmp(ss, "rearchive", len))
				work = REARCHIVE;
			else if (!memcmp(ss, "RPrearchive", len))
				work = REARCHIVErl;
			else if (!memcmp(ss, "cloudpatch", len))
				work = CLOUDPATCH;
			else
			{
				fprintf(stderr, "unknown argument '%.*s'\n", len, ss);
				exit(EXIT_FAILURE);
			}
		}
		
		else if (times == 2)
		{
			switch (work)
			{
				/* if='x.z64' */
				case IF:
					if (Aif)
						FERR("'if' arg provided more than once");
					Aif = ss;
					rom = rom_new(0, Aif);
					break;
				
				/* of='x-comp.z64' */
				case OF:
					if (Aof)
						FERR("'of' arg provided more than once");
					Aof = ss;
					break;
				
				/* codec='yaz' */
				case CODEC:
					if (Acodec)
						FERR("'codec' arg provided more than once");
					Acodec = ss;
					break;
				
				/* dma='0xStart,0xNumEntries' */
				case DMA:
					if (!Acodec)
						FERR("'dma' arg provided before 'codec' arg");
					if (!Aif)
						FERR("'dma' arg provided before 'if' arg");
					if (Adma)
						FERR("'dma' arg provided more than once");
					Adma = ss;
					if (sscanf(ss, "%X,%X", &dma_start, &dma_num) != 2)
						FERR("'dma' arg bad formatting");
					rom_dma(rom, dma_start, dma_num);
					break;
				
				/* mb='%d' */
				case MB:
					if (mb)
						FERR("'mb' arg provided more than once");
					if (sscanf(ss, "%d", &mb) != 1)
						FERR("'mb' argument could not get value");
					if (mb <= 0)
						FERR("'mb' invalid value");
					break;
				
				/* compress='which' */
				case COMPRESS:
					if (!Adma)
						FERR("'compress' arg provided before 'dma' arg");
					which(ss, compress);
					break;
				
				/* NOcompress='which' */
				case COMPRESSno:
					if (!Adma)
						FERR("'NOcompress' arg provided before 'dma' arg");
					which(ss, compress_no);
					break;
				
				/* rearchive='which' */
				case REARCHIVE:
					if (!Adma)
						FERR("'rearchive' arg provided before 'dma' arg");
					which(ss, rearchive);
					break;
				
				/* RLrearchive='which' */
				case REARCHIVErl:
					if (!Adma)
						FERR("'RLrearchive' arg provided before 'dma' arg");
					which(ss, rearchive_rl);
					break;
				
				/* cloudpatch='patch.txt' */
				case CLOUDPATCH:
					if (!rom)
						FERR("'cloudpatch' arg provided before 'if' arg");
					rom_cloudpatch(rom, 0, ss);
					break;
				
				/* default */
				case NONE:
					FERR("internal error");
					break;
			}
			
			work = 0;
		}
		
		times += 1;
		if (times == 4)
			times = 0;
	}
	
	if (!Aof)
		FERR("no 'of' arg provided");
	
	if (!mb)
		FERR("no 'mb' arg provided");
	
	if (!Acodec)
		FERR("no 'codec' arg provided");
	
	/* finished initializing dma settings */
	rom_dma_ready(rom);
	
	/* compress rom */
	rom_compress(rom, Acodec, mb);
	
	/* write compressed rom */
	rom_save(rom, Aof);
	
	/* cleanup */
	rom_free(rom);
	free(argstring);
	
	fprintf(stderr, "rom compressed successfully!\n");
	
	zzrtl_waitkey();
	
	exit(EXIT_SUCCESS);
}

