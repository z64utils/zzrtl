#include <stdlib.h>

#include "zzrtl.h"

void main_cloudpatch(int argc, char **argv)
{
	struct rom *rom;
	int i;
	
	char *in = argv[1];
	char *out = argv[2];
	
	/* load in rom */
	rom = rom_new(0, in);
	
	/* apply every cloudpatch */
	for (i = 3; i < argc; ++i)
		rom_cloudpatch(rom, 0, argv[i]);
	
	/* save new rom */
	rom_save(rom, out);
	
	/* cleanup */
	rom_free(rom);
	exit(EXIT_SUCCESS);
}

