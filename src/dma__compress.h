
static
inline
void
display_compression_progress(char *enc, int v, int total)
{
	/* caching enabled */
	if (g_use_cache)
		fprintf(
			stderr
			, "\r""updating 'cache/%s' %d/%d: "
			, enc
			, v
			, total
		);
	
	else
		fprintf(
			stderr
			, "\r""compressing file %d/%d: "
			, v
			, total
		);
}


static
void
dma__compress(
	struct rom *rom
	, unsigned char *compbuf
	, int encfunc(
		void *src
		, unsigned int src_sz
		, void *dst
		, unsigned int *dst_sz
		, void *_ctx
	)
	, char *enc
	, char *slash_enc
	, struct folder *list
	, int stride   /* number of entries to advance each time */
	, int ofs      /* entry in list to start at */
	, int report   /* print info to stderr etc (last thread only) */
	, void *ctx    /* compression context */
)
{
	struct dma *dma;
	struct fldr_item *item;
//	int compbuf_along = 0;
	
	for (dma = rom->dma + ofs; dma - rom->dma < rom->dma_num; dma += stride)
	{
		unsigned char *data = rom->data + dma->start;
		unsigned char checksum[64];
		char readable[64];
		int len = dma->end - dma->start;
		
//		/* don't compress files specifically marked */
//		if (!dma->compress)
//			goto dont_compress;
	
		/* report the progress */
		if (report)
		{
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
			/*display_compression_progress(
				enc
				//, dma - rom->dma
				//, rom->dma_num-1
				, min_int(dma - rom->dma, rom->injected)
				, rom->injected//rom->dma_num - 1
			);*/
			
#if 0
			if (dma - rom->dma == rom->dma_num - 1)
			{
				fprintf(stderr, "hold on");
#if 0
				const char *flavor[] = {
					"Please wait a moment"
					, "Hold on a sec"
					, "Wait a moment"
					, "Loading"
					, "Now working"
					, "Now constructing"
					, "It's not broken!"
					, "Coffee break!"
					, "Please set the B side"
					, "Be patient"
					, "Now, please wait a moment"
					, "Don't panic, don't panic! Take a break, take a break."
				};
#endif
			}
#endif
		}
		
		/* skip files that have a size of 0 */
		if (dma->start == dma->end)
		{
//			dma->done = 1;
			continue;
		}
		
		/* caching is disabled, just compress */
		if (!g_use_cache)
		{
			int err;
			dma->compbuf = compbuf;// + compbuf_along;
			
			/* don't compress this file */
			if (!dma->compress)
			{
				dma->compSz = dma->end - dma->start;
				dma->compbuf =
					memdup_safe(rom->data + dma->start, dma->compSz)
				;
//				memcpy(dma->compbuf, rom->data + dma->start, dma->compSz);
//				compbuf_along += dma->compSz;
//				dma->done = 1;
				continue;
			}
			
			err =
			encfunc(
				rom->data + dma->start
				, dma->end - dma->start
				, dma->compbuf
				, &dma->compSz
				, ctx
			);
			
			/* file doesn't benefit from compression */
			if (dma->compSz >= dma->end - dma->start)
			{
				dma->compSz = dma->end - dma->start;
				dma->compbuf =
					memdup_safe(rom->data + dma->start, dma->compSz)
				;
				//memcpy(dma->compbuf, rom->data + dma->start, dma->compSz);
				dma->compress = 0;
			}
			else
				dma->compbuf =
					memdup_safe(dma->compbuf, dma->compSz)
				;
			
//			comp_total += dma->compSz;
			
			//fprintf(stdout, "%08X: %08X (uSz %08X)\n", dma - rom->dma, dma->compSz, dma->end - dma->start);
			
			if (err)
				die_i("compression error");
			
			/* advance along */
//			compbuf_along += dma->compSz;
			
			/* the rest of the loop only applies for caches */
//			dma->done = 1;
			continue;
		}
		
		/* get readable checksum name */
		stb_sha1(checksum, data, len);
		stb_sha1_readable(readable, checksum);
		
		char *iname = 0;
		
		/* see if item already exists in folder */
		item = folder_find_name_n_ext(list, readable);
		if (item)
		{
			/* use full file name, including extension */
			iname = item->name;
			/* it exists, so use udata to mark the file as used */
			item->udata = slash_enc;
			dma->compSz = file_size(iname);
			/* uncompressed file */
			if (strstr(iname, ".raw"))
				dma->compress = 0;
		}
		/* item doesn't exist, so create it */
		else
		{
			//fprintf(stderr, "'%s' does not exist in list\n", readable);
			char *ss;
			int err;
			unsigned char *out = compbuf;
			unsigned int out_sz;
		
			if (!dma->compress)
			{
				out = rom->data + dma->start;
				out_sz = dma->end - dma->start;
				dma->compress = 0;
				strcat(readable, ".raw");
				
				/* write file */
				if (file_write(readable, out, out_sz) != out_sz)
					die_i("error writing file 'cache/%s/%s'", enc, readable);
				
				dma->compSz = out_sz;
//				comp_total += dma->compSz;
				dma->compname = strdup_safe(readable);
				continue;
			}
				
			err =
			encfunc(
				rom->data + dma->start
				, dma->end - dma->start
				, out
				, &out_sz
				, ctx
			);
			
			//fprintf(stdout, "%08X: %08X (uSz %08X)\n", dma - rom->dma, out_sz, dma->end - dma->start);
			
			if (err)
				die_i("compression error");
			
			/* file doesn't benefit from compression */
			if (out_sz >= dma->end - dma->start)
			{
				out = rom->data + dma->start;
				out_sz = dma->end - dma->start;
				dma->compress = 0;
				strcat(readable, ".raw");
			}
			/* file benefits from compression */
			else
				/* add encoding as extension, ex ".yaz" */
				strcat(readable, slash_enc);
			
			/* write file */
			if (file_write(readable, out, out_sz) != out_sz)
				die_i("error writing file 'cache/%s/%s'", enc, readable);
			
			dma->compSz = out_sz;
			iname = readable;
		}
//		comp_total += dma->compSz;
		
		/* back up compressed name for each, to avoid
		   having to re-checksum later */
		dma->compname = strdup_safe(iname);
	}
}


static
void *
dma__compress_threadfunc(void *_CT)
{
	struct compThread *CT = _CT;
	
	dma__compress(
		CT->rom
		, CT->data
		, CT->encfunc
		, CT->enc
		, CT->slash_enc
		, CT->list
		, CT->stride /* stride */
		, CT->ofs    /* ofs    */
		, CT->report /* report */
		, CT->ctx
	);
	
	return 0;
}


static
void
dma__compress_thread(
	struct compThread *CT
	, struct rom *rom
	, unsigned char *compbuf
	, int encfunc(
		void *src
		, unsigned int src_sz
		, void *dst
		, unsigned int *dst_sz
		, void *_ctx
	)
	, char *enc
	, char *slash_enc
	, struct folder *list
	, int stride   /* number of entries to advance each time */
	, int ofs      /* entry in list to start at */
	, int report   /* print info to stderr etc (last thread only) */
	, void *ctx    /* compression context */
)
{
	CT->rom = rom;
	CT->data = compbuf;
	CT->encfunc = encfunc;
	CT->enc = enc;
	CT->slash_enc = slash_enc;
	CT->list = list;
	CT->stride = stride;
	CT->ofs = ofs;
	CT->report = report;
	CT->ctx = ctx;
	
	if (pthread_create(&CT->pt, 0, dma__compress_threadfunc, CT))
		die_i("threading error");
}

