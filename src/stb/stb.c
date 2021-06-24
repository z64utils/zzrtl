#include <stdlib.h>  /* free */
#include <string.h>  /* memcpy, for rnd.h */
#include "../zzrtl.h"   /* malloc_safe, realloc_safe */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#define RND_IMPLEMENTATION

#if 0
#define STBI_ONLY_PNG

#define STBI_MALLOC    malloc_safe
#define STBI_REALLOC   realloc_safe
#define STBI_FREE      free

#define STBIW_MALLOC   malloc_safe
#define STBIW_REALLOC  realloc_safe
#define STBIW_FREE     free

#include "stb/stb.h"
#else
#include "../wow.h"

	#define STBI_FAILURE_USERMSG 1

	#define STBI_ONLY_PNG

	#define STBI_MALLOC    malloc_safe
	#define STBI_REALLOC   realloc_safe
	#define STBI_FREE      free

	#define STBIW_MALLOC   malloc_safe
	#define STBIW_REALLOC  realloc_safe
	#define STBIW_FREE     free
	
	#undef fread
	#undef fwrite
	#define fread wow_fread
	#define fwrite wow_fwrite

	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#define STBI_NO_BMP
	#define STBI_NO_TGA
	#define STBI_NO_HDR
	#define STBI_NO_JPEG
	//#define STBIW_WINDOWS_UTF8
	#define stbiw__fopen wow_fopen
	#include "stb_image_write.h"

	#define STB_IMAGE_IMPLEMENTATION
	#define STBI_ONLY_PNG
	//#define STBI_WINDOWS_UTF8
	#define stbi__fopen wow_fopen
	#include "stb_image.h"
#endif


#include <stdint.h>
#define RND_U32 uint32_t
#define RND_U64 uint64_t
#define RND_WANT_PCG
#include "../rnd.h"

