#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h> /* toupper */

/* POSIX dependencies */
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define WOW_IMPLEMENTATION
#include "wow.h"

#include "zzrtl.h"
#include "preproc.h"
#include "rnd.h" /* rnd_pcg */

#undef   fopen
#undef   fread
#undef   fwrite
#define  fopen   wow_fopen
#define  fread   wow_fread
#define  fwrite  wow_fwrite

/* r3 extension checking: confirm extension is .rtl */

/* bird of the day
 * XXX do not edit this; we want bird of the day to remain consistent
 *     between revisions
 */
static
char *
botd(void)
{
	time_t rawtime;
	struct tm *tm;
	char *buf;
	char *birdname;
	const char *states[] = {"AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA", "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD", "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ", "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC", "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY"};
	const char *metal[] = {
#if 0
		"galvanized"
		, "magnesium"
		, "aluminum"
		, "titanium"
		, "chrome"
		, "iron"
		, "cobalt"
		, "nickel"
		, "copper"
		, "zinc"
		, "silver"
		, "tin"
		, "tungsten"
		, "golden"
		, "radium"
		, "uranium"
		, "platinum"
		, "steel"
#endif
		"alloy"
		, "aluminum"
		, "anvil" // a metal block
		, "brass"
		, "bronze"
		, "bullion" // gold or silver in the form of solid bars
		, "burnished" // to rub metal until it shines (-ed)
		//, "carat" // a unit for measuring how pure gold is
		, "crucible"
		, "carbon"
		, "chromium"
		, "cobalt"
		, "copper"
		, "ferric" // ferricfowl
		, "foil"
		, "foundry" // a factory where metal or glass is heated
		, "gilded"
		//, "gilt" // a thin layer of gold or something like gold
		, "golden"
		, "galvanized"
		, "iron"
		, "ingot" // a block of gold, silver, or other metal
		, "karat" // unit for measuring gold purity
		, "lead"
		, "magnesium"
		, "mercury"
		, "nickel"
		, "nugget" // a rough lump of gold or other metal
		, "ore" // mineral or aggregate thereof, containing value (esp. metal)
		, "pewter"
		, "platinum"
		, "radium"
		, "reforged"
		, "shiny"
		, "shining"
		, "silver"
		, "solder"
		, "smelted" // to heat rock in order to remove the metal it contains (-ed)
		, "stainless"
		, "steel"
		, "sterling"
		, "tarnished"
		, "tempered" // to make steel hard
		, "tin"
		, "titanium"
		, "tungsten"
		, "uranium"
		, "welded" // to join two pieces of metal by heating
		, "zinc"
	};
	
	const char *bird[] = {
	//	"tinamous",
	//	"moa",
		"ostrich",
	//	"rhea",
		"cassowary",
		"emu",
		"kiwi",
	//	"elephant-bird",
		"parrot",     /* parrots */
		"parakeet",
		"macaw",
		"cockatoo",
		"cockatiel",
		"duck",       /* waterfowl */
		"goose",
		"swan",
		"pheasant",   /* turkeys */
		"turkey",
		"grouse",
		"quail",
		"partridge",
		
		"penguin",
		"loon",
		
		"albatross",
		"seagull",
		"heron",
		"pelican",
		"stork",
		
		"flamingo",
		
		"vulture",
		"hawk",
		"eagle",
		"buzzard",
		"falcon",
		
		"sandpiper",
		"owl",
		
		"pigeon",
		"dove",
		
		"hummingbird",
		
		//"kingfisher",
		"hornbill",
		//"puffbird",
		"toucan",
		"woodpecker",
		
		"chicken",
		"fowl",
		"bird",
		
		"crow",
		"raven",
		"lark",
		"mockingbird",
		"sparrow",
		
		"wren",
		
		"cucco", // oot thanks
		"guay",
		"keese",
		"cojiro",
		"kargarok", // tp
		"takkuri", // mm
	};
	
	buf = malloc_safe(256);
	buf[0] = '\0';
	
	time(&rawtime);
	tm = localtime(&rawtime);
	
	/* initialize rand() based on day since jan 1 1900 */
	uint32_t /*unsigned long long*/ seed = tm->tm_year * 366 + tm->tm_yday;
	//srand(seed);
	rnd_pcg_t pcg;
	rnd_pcg_seed(&pcg, seed);
	
	uint32_t wstate = (rnd_pcg_next(&pcg) + seed) % (sizeof(states)/sizeof(states[0]));
	uint32_t wmetal = (rnd_pcg_next(&pcg) + seed) % (sizeof(metal)/sizeof(metal[0]));
	uint32_t wbird  = (rnd_pcg_next(&pcg) + seed) % (sizeof(bird)/sizeof(bird[0]));
	
	strcat(buf, states[wstate]);
	strcat(buf, metal[wmetal]);
	birdname = buf + strlen(buf);
	strcat(buf, bird[wbird]);
	buf[2] = toupper(buf[2]);
	*birdname = toupper(*birdname);
#if 0
	for (int i=0; i < (sizeof(states)/sizeof(states[0])); ++i)
		fprintf(stderr, "state: '%s'\n", states[i]);
	for (int i=0; i < (sizeof(metal)/sizeof(metal[0])); ++i)
		fprintf(stderr, "metal: '%s'\n", metal[i]);
	for (int i=0; i < (sizeof(bird)/sizeof(bird[0])); ++i)
		fprintf(stderr, "bird: '%s'\n", bird[i]);
#endif
	return buf;
}

static
void
zzrtl(void)
{
	char *bird = botd();
	fprintf(
		stderr, 
		"/*************************\n"
		" * zzrtl v1.0.5 <z64.me> *\n"
		" *************************/\n"
	);
	fprintf(
		stderr
		, "  \\./  bird of the day: %s\n"
		, bird
	);
	fprintf(stderr, "\n");
	free(bird);
}

static
void
showargs_cmd(void)
{
#define PFX " / "
	die(
		PFX"info for command line users:\n"
		PFX"usage example: zzrtl path/to/script.rtl\n"
		PFX"any arguments starting with -- that precede the .rtl\n"
		PFX"allow you to set additional flags; available options:\n"
		PFX"--help\n"
		PFX"  show this documentation and exit immediately\n"
		PFX"--nowaiting or --no_waiting\n"
		PFX"  disable 'press enter key to exit' prompt (Windows)\n"
		PFX"--nocache or --no_cache\n"
		PFX"  disable cache creation/use during compression\n"
		PFX"--kb=256\n"
		PFX"  set memory pool size (256 is the default)\n"
		PFX"  [!] you shouldn't need this unless unexplainable\n"
		PFX"      problems that aren't your fault start to happen\n"
		PFX"--cloudpatch in.z64 out.z64 patch.txt ... patch-n.txt\n"
		PFX"  apply cloudpatches to in.z64, save result as out.z64\n"
		PFX"--compress \"more args...\"\n"
		PFX"  use zzrtl as a standalone rom compressor\n"
		PFX"  other settings arguments must precede this one\n"
		PFX"  this argument wraps more arguments:\n"
		PFX"    if='input-rom.z64'\n"
		PFX"    of='output-rom.z64'\n"
		PFX"   *cloudpatch='patch.txt' (apply cloudpatch)\n"
		PFX"   *cloudpatch='patch1.txt' (multiple are allowed)\n"
		PFX"    codec='yaz' or 'ucl' etc.\n"
		PFX"    dma='0xStart,0xNumEntries'\n"
		PFX"    mb='32'\n"
		PFX"    compress='which' (enable compression on files)\n"
		PFX"   *NOcompress='which' (disable compression on files)\n"
		PFX"   *rearchive='which' (reencode MM archives)\n"
		PFX"   *RPrearchive='which' (rearchive w/ repack enabled)\n"
		PFX"    [*] which can be this complex: '10-14,23,24,31-END'\n"
		PFX"        or this simple: '1127'\n"
		PFX"    [*] END is shorthand for the last dma index\n"
		PFX"    [*] arguments marked with * are optional'\n"
		PFX"    [!] archive repack break MM without patches;\n"
		PFX"        you shouldn't need this feature...\n"
		PFX"    [!] the 'extra quotes' are necessary; don't forget\n"
		PFX"all arguments starting at the .rtl are available for use\n"
		PFX"inside the script using argc, argv (the .rtl is argv[0])\n"
		PFX"ex: zzrtl --nowaiting path/to/script.rtl yaz\n"
		"\n"
	);
}

static
void
showargs(int argc)
{
	if (argc == 1)
	{
#ifdef WIN32
#if 0
		die(
			PFX"This is not the kind of program you double-click, \n"
			PFX"you Windows user, you! You must drag-and-drop an \n"
			PFX".rtl script onto the executable. Alternatively,  \n"
			PFX"you can right-click the .rtl, select 'Open with', \n"
			PFX"and set up your system to always associate such \n"
			PFX"files with this program, so you can execute them \n"
			PFX"by simply double-clicking them. If you are still \n"
			PFX"lost, read the starter guide on www.z64.me\n\n"
		);
#elseif 0
die(
//  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
PFX"This is not the kind of program you double-click, you Windows\n"
PFX"user, you! You must drag-and-drop the .rtl script onto the .exe.\n"
PFX"Alternatively, you can right-click the .rtl, select 'Open with',\n"
PFX"and set it up to always associate such files with zzrtl, so you\n"
PFX"you can execute them by simply double-clicking them. If you are\n"
PFX"still lost, read the starter guide on www.z64.me"
);
#else
die(
//  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
PFX"This is not the kind of program you double-click, you Windows\n"
PFX"user, you! Right-click any .rtl script, select 'Open with',\n"
PFX"and set it up to always associate .rtl files with zzrtl. Then,\n"
PFX"you can execute .rtl files by simply double-clicking them. If\n"
PFX"you are still lost, read the starter guide on www.z64.me\n\n"
PFX"(command line users: run zzrtl --help for goodies)"
);
#endif
#endif
	}
	die("[!] no input .rtl provided (try zzrtl --help)");
#undef PFX
}
/* end */

typedef int REGXC_INT; /* regular int, for funcs that require them */

//#define int long long // to work with 64bit address
#define int intptr_t // to work with 64bit address

#ifndef NDEBUG
int debug;    // print the executed instructions
int assembly; // print out the assembly and source
#endif

int token; // current token

// instructions
enum {
LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,
DIV ,MOD ,OPEN,READ,CLOS,FPRT,SPRT,SSCF,PRTF,MALC,FREE,MSET,MCMP,MCPY,SSTM,
SCAT,SCMP,SCCM,EXIT
/* extended instructions */
/* rom */
	, ZZXC_ROM_NEW
	, ZZXC_ROM_FREE
	, ZZCX_ROM_RAW
	, ZZXC_ROM_SAVE
	, ZZXC_ROM_ALIGN
	, ZZXC_ROM_COMPRESS
	, ZZXC_ROM_SEEK
	, ZZXC_ROM_SEEK_CUR
	, ZZXC_ROM_TELL
	, ZZXC_ROM_SIZE
	, ZZXC_ROM_INJECT
	, ZZXC_ROM_EXTRACT
	, ZZXC_ROM_FILE_START
	, ZZXC_ROM_FILE_END
	, ZZXC_ROM_FILE_SZ
	, ZZXC_ROM_FILE_DMA
	, ZZXC_ROM_FILE
	, ZZXC_ROM_INJECT_PNG
	, ZZXC_ROM_INJECT_RAW
	, ZZXC_ROM_EXTRACT_PNG
	, ZZXC_ROM_WRITE
	, ZZXC_ROM_WRITE32
	, ZZXC_ROM_WRITE24
	, ZZXC_ROM_WRITE16
	, ZZXC_ROM_WRITE8
	, ZZXC_ROM_READ32
	, ZZXC_ROM_READ24
	, ZZXC_ROM_READ16
	, ZZXC_ROM_READ8
	, ZZXC_ROM_DMA
	, ZZXC_ROM_DMA_QUEUE_ONE
	, ZZXC_ROM_DMA_QUEUE
	, ZZXC_ROM_DMA_COMPRESS_ONE
	, ZZXC_ROM_DMA_COMPRESS
	, ZZXC_ROM_DMA_REARCHIVE_ONE
	, ZZXC_ROM_DMA_REARCHIVE
	, ZZXC_ROM_DMA_READY
	, ZZXC_rom_dma_register
	, ZZXC_rom_dma_prefix
	, ZZXC_rom_inject_prefix
	, ZZXC_rom_inject_mode
	, ZZXC_rom_dma_offset
	, ZZXC_ROM_CLOUDPATCH
	, ZZXC_rom_inject_raw_dma
	, ZZXC_rom_inject_dma
	, ZZXC_rom_extract_dma
	, ZZXC_rom_filename
	, ZZXC_rom_flag
	//, ZZXC_ROM_WRITABLE
/* generic */
	, ZZXC_DIE
	, ZZXC_PUT32
	, ZZXC_PUT24
	, ZZXC_PUT16
	, ZZXC_PUT8
	, ZZXC_GET32
	, ZZXC_GET24
	, ZZXC_GET16
	, ZZXC_GET8
	, ZZXC_U8
	, ZZXC_S8
	, ZZXC_U16
	, ZZXC_S16
	, ZZXC_U32OP
	, ZZXC_SUBSTRING
	, ZZXC_FIND_BYTES
	, ZZXC_FIND_BYTES_STRIDE
	, ZZXC_FIND_TEXT
	, ZZXC_FIND_TEXT_STRIDE
	, ZZXC_OVL_VRAM_SZ
	, ZZXC_LOAD_PNG
	, ZZXC_int_array
	, ZZXC_new_string
	, ZZXC_string_list_file
	, ZZXC_loadfile
	, ZZXC_tsv_col_row
/* directory */
	, ZZXC_DIR_EXISTS
	, ZZXC_DIR_ENTER
	, ZZXC_DIR_ENTER_FILE
	, ZZXC_DIR_LEAVE
	, ZZXC_DIR_MKNAME
	, ZZXC_DIR_USE_STYLE
	, ZZXC_FILE_EXISTS
/* folder */
	, ZZXC_FOLDER_NEW
	, ZZXC_FOLDER_FREE
	, ZZXC_FOLDER_NAME
	, ZZXC_FOLDER_INDEX
	, ZZXC_FOLDER_NEXT
	, ZZXC_FOLDER_COUNT
	, ZZXC_FOLDER_REMAINING
	, ZZXC_FOLDER_MAX
/* conf */
	, ZZXC_CONF_NEW
	, ZZXC_CONF_FREE
	, ZZXC_CONF_GET_XC_INT
	, ZZXC_CONF_GET_STR
	, ZZXC_CONF_NEXT
	, ZZXC_CONF_REMAINING
	, ZZXC_CONF_EXISTS
	, ZZXC_CONF_NAME
	, ZZXC_CONF_VALUE
/* num instructions */
	, INSTRUCTION_COUNT
};

#define ZZXC_FUNC_NAME_STR \
/* rom */ \
" rom_new " \
" rom_free " \
" rom_raw " \
" rom_save " \
" rom_align " \
" rom_compress " \
" rom_seek " \
" rom_seek_cur " \
" rom_tell " \
" rom_size " \
" rom_inject " \
" rom_extract " \
" rom_file_start " \
" rom_file_end " \
" rom_file_sz " \
" rom_file_dma " \
" rom_file " \
" rom_inject_png " \
" rom_inject_raw " \
" rom_extract_png " \
" rom_write " \
" rom_write32 " \
" rom_write24 " \
" rom_write16 " \
" rom_write8 " \
" rom_read32 " \
" rom_read24 " \
" rom_read16 " \
" rom_read8 " \
" rom_dma " \
" rom_dma_queue_one " \
" rom_dma_queue " \
" rom_dma_compress_one " \
" rom_dma_compress " \
" rom_dma_rearchive_one " \
" rom_dma_rearchive " \
" rom_dma_ready " \
" rom_dma_register " \
" rom_dma_prefix " \
" rom_inject_prefix " \
" rom_inject_mode " \
" rom_dma_offset " \
" rom_cloudpatch " \
" rom_inject_raw_dma " \
" rom_inject_dma " \
" rom_extract_dma " \
" rom_filename " \
" rom_flag " \
/*" rom_writable "*/ \
/* generic */ \
" die " \
" put32 " \
" put24 " \
" put16 " \
" put8 " \
" get32 " \
" get24 " \
" get16 " \
" get8 " \
" u8 " \
" s8 " \
" u16 " \
" s16 " \
" u32op " \
" substring " \
" find_bytes " \
" find_bytes_stride " \
" find_text " \
" find_text_stride " \
" ovl_vram_sz " \
" load_png " \
" int_array " \
" new_string " \
" string_list_file " \
" loadfile " \
" tsv_col_row " \
/* directory */ \
" dir_exists " \
" dir_enter " \
" dir_enter_file " \
" dir_leave " \
" dir_mkname " \
" dir_use_style " \
" file_exists " \
/* folder */ \
" folder_new " \
" folder_free " \
" folder_name " \
" folder_index " \
" folder_next " \
" folder_count " \
" folder_remaining " \
" folder_max " \
/* conf */ \
" conf_new "\
" conf_free "\
" conf_get_int "\
" conf_get_str "\
" conf_next "\
" conf_remaining "\
" conf_exists "\
" conf_name "\
" conf_value "\

#define ZZXC_FUNC_INST_STR \
/* rom */ \
	",RMNW" /* rom_new */ \
	",RMFR" /* rom_free */ \
	",RMRW" /* rom_raw */ \
	",RMSV" /* rom_save */ \
	",RMAL" /* rom_align */ \
	",RMCM" /* rom_compress */ \
	",RMSK" /* rom_seek */ \
	",RMSC" /* rom_seek_cur */ \
	",RMTL" /* rom_tell */ \
	",RMSZ" /* rom_size */ \
	",RMIJ" /* rom_inject */ \
	",RMEX" /* rom_extract */ \
	",RMFS" /* rom_file_start */ \
	",RMFE" /* rom_file_end */ \
	",RMFZ" /* rom_file_sz */ \
	",RMFD" /* rom_file_dma */ \
	",RMFL" /* rom_file */ \
	",RMIP" /* rom_inject_png */ \
	",RMIR" /* rom_inject_raw */ \
	",RMEP" /* rom_extract_png */ \
	",RMWT" /* rom_write */ \
	",RMW4" /* rom_write32 */ \
	",RMW3" /* rom_write24 */ \
	",RMW2" /* rom_write16 */ \
	",RMW1" /* rom_write8 */ \
	",RMR4" /* rom_read32 */ \
	",RMR3" /* rom_read24 */ \
	",RMR2" /* rom_read16 */ \
	",RMR1" /* rom_read8 */ \
	",RMDM" /* rom_dma */ \
	",RMDQ" /* rom_dma_queue_one */ \
	",RMDT" /* rom_dma_queue */ \
	",RMDC" /* rom_dma_compress_one */ \
	",RMDK" /* rom_dma_compress */ \
	",RMDA" /* rom_dma_rearchive_one */ \
	",RMDr" /* rom_dma_rearchive */ \
	",RMDR" /* rom_dma_ready */ \
	",RMDX" /* rom_dma_register */ \
	",RMDP" /* rom_dma_prefix */ \
	",RMPX" /* rom_inject_prefix */ \
	",RMIM" /* rom_inject_mode */ \
	",RMDO" /* rom_dma_offset */ \
	",RMCP" /* rom_cloudpatch */ \
	",RMRD" /* rom_inject_raw_dma */ \
	",RMFD" /* rom_inject_dma */ \
	",RMED" /* rom_extract_dma */ \
	",RMFN" /* rom_filename */ \
	",RMFL" /* rom_flag */ \
	/*",RMWR"*/ /* rom_writable */ \
/* generic */ \
	",DIE " /* die */ \
	",PUT4" /* put32 */ \
	",PUT3" /* put24 */ \
	",PUT2" /* put16 */ \
	",PUT1" /* put8 */ \
	",GET4" /* get32 */ \
	",GET3" /* get24 */ \
	",GET2" /* get16 */ \
	",GET1" /* get8 */ \
	",U8  " /* u8 */ \
	",S8  " /* s8 */ \
	",U16 " /* u16 */ \
	",S16 " /* s16 */ \
	",U32O" /* u32op */ \
	",SSTR" /* substring */ \
	",FNB " /* find_bytes */ \
	",FNBS" /* find_bytes_stride */ \
	",FNT " /* find_text */ \
	",FNTS" /* find_text_stride */ \
	",OVLS" /* ovl_vram_sz */ \
	",LPNG" /* load_png */ \
	",U32A" /* int_array */ \
	",NSTR" /* new_string */ \
	",FSTR" /* string_list_file */ \
	",LFIL" /* loadfile */ \
	",TVCR" /* tsv_col_row */ \
/* directory */ \
	",DRXS" /* dir_exists */ \
	",DREN" /* dir_enter */ \
	",DREF" /* dir_enter_file */ \
	",DRLV" /* dir_leave */ \
	",DRMN" /* dir_mkname */ \
	",DRST" /* dir_use_style */ \
	",FXST" /* file_exists */ \
/* folder */ \
	",FLNW" /* folder_new */ \
	",FLFR" /* folder_free */ \
	",FLNM" /* folder_name */ \
	",FLIX" /* folder_index */ \
	",FLNX" /* folder_next */ \
	",FLCT" /* folder_count */ \
	",FLRM" /* folder_remaining */ \
	",FLMX" /* folder_max */ \
/* conf */ \
	",CFNW" /* conf_new */ \
	",CFFR" /* conf_free */ \
	",CFGI" /* conf_get_int */ \
	",CFGS" /* conf_get_str */ \
	",CFNX" /* conf_next */ \
	",CFRM" /* conf_remaining */ \
	",CFEX" /* conf_exists */ \
	",CFNM" /* conf_name */ \
	",CFVL" /* conf_value */ \

// tokens and classes (operators last and in precedence order)
// copied from c4
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// fields of identifier
enum {Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize};


// types of variable/function
enum { XC_CHAR, XC_INT, XC_PTR };

// type of declaration.
enum {Global, Local};

int  *text, *Otext      // text segment
	, *stack, *Ostack;   // stack
int  *old_text;         // for dump text segment
char *data, *Odata;     // data segment
int  *idmain;

char *src, *old_src, *Osrc;  // pointer to source code string;

int poolsize; // default size of text/data/stack
int *pc, *bp, *sp, ax, cycle; // virtual machine registers

int *current_id  // current parsed ID
	, *symbols    // symbol table
	, *Osymbols
	, line        // line number of source code
	, token_val;  // value of current token (mainly for number)

int basetype;    // the type of a declaration, make it global for convenience
int expr_type;   // the type of an expression

// function frame
//
// 0: arg 1
// 1: arg 2
// 2: arg 3
// 3: return address
// 4: old bp pointer  <- index_of_bp
// 5: local var 1
// 6: local var 2
int index_of_bp; // index of bp pointer on stack

char* getlines(char* dest, char* str, int lineA, int lineB) {
	char buffer[1024 * 4] = { 0 };
	int wp = 0;
	int lastLinePrint = 0;
	int line = 1;
	int processed = 0;
	int i = 0;
	int strsz;
	int strlng = strlen(str);

	while (!processed || line < lineB) {
		if (str[i] == '\n') {
			line++;
			i++;
		}

		if (line == lineA + processed && line != lastLinePrint) {
			strsz = 0;
			while (str[i + strsz] != '\n') {
				strsz++;
				if ((i + strsz) > strlng)
					return;
			}
			memmove(&buffer[wp], &str[i], strsz);
			wp = strsz;
			lastLinePrint = line;
			processed++;
			i++;
			continue;
		}

		i++;

		if (i > strlng)
			return;
	}

	memmove(dest, buffer, wp);
}

void printlines(char* str, int lineA, int lineB) {
	char buffer[1024];
	int lastLinePrint = 0;
	int line = 1;
	int processed = 0;
	int i = 0;
	int strsz;
	int strlng = strlen(str);

	while (!processed || line < lineB) {
		if (str[i] == '\n') {
			line++;
			i++;
		}

		if (line == lineA + processed && line != lastLinePrint) {
			strsz = 0;
			while (str[i + strsz] != '\n') {
				strsz++;
				if ((i + strsz) > strlng)
					return;
			}
			bzero(buffer, 1024);
			memmove(buffer, &str[i], strsz);
			printf("%s\n", buffer);
			lastLinePrint = line;
			processed++;
			i++;
			continue;
		}

		i++;

		if (i > strlng)
			return;
	}
}

void errorinfo_line() {
	char buffer[1024 * 2] = { 0 };
	intptr_t target = 0;
	int i = 0;
	int j = 12;
	int strlength = 0;

	printf("\a[!]: Error!\n");
	int a, b;
	a = line - 5;
	a = a < 0 ? 0 : a;
	b = line - 2;
	b = b < 0 ? 0 : b;

	if (a && b) {
		printf("\e[90m");
		printlines(old_src, line - 5, line - 1);
	}
	printf("\e[91m");
	printlines(old_src, line, line);
	printf("\e[90m");
	printlines(old_src, line + 1, line + 12);
	printf("\e[0m");
}

void next() {
	char *last_pos;
	int hash;

	while ((token = *src)) {
		++src;

		if (token == '\n') {
#ifndef NDEBUG
			if (assembly) {
				// print compile info
				printf("%d: %.*s", line, src-old_src, old_src);
				old_src = src;

				while (old_text < text) {
					printf("%8.4s", & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
"OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
"OPEN,READ,CLOS,FPRT,SPRT,SSCF,PRTF,MALC,FREE,MSET,MCMP,MCPY,SSTM,SCAT,SCMP,SCCM,EXIT"
ZZXC_FUNC_INST_STR
[*++old_text * 5]);

					if (*old_text <= ADJ)
						printf(" %d\n", *++old_text);
					else
						printf("\n");
				}
			}
#endif
			++line;
		}
		else if (token == '#') {
			// skip macro, because we will not support it
			while (*src != 0 && *src != '\n') {
				src++;
			}
		}
		else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')) {

			// parse identifier
			last_pos = src - 1;
			hash = token;

			while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')) {
				hash = hash * 147 + *src;
				src++;
			}

			// look for existing identifier, linear search
			current_id = symbols;
			while (current_id[Token]) {
				if (current_id[Hash] == hash && !memcmp((char *)current_id[Name], last_pos, src - last_pos)) {
					//found one, return
					token = current_id[Token];
					return;
				}
				current_id = current_id + IdSize;
			}


			// store new ID
			current_id[Name] = (int)last_pos;
			current_id[Hash] = hash;
			token = current_id[Token] = Id;
			return;
		}
		else if (token >= '0' && token <= '9') {
			// parse number, three kinds: dec(123) hex(0x123) oct(017)
			token_val = token - '0';
			if (token_val > 0) {
				// dec, starts with [1-9]
				while (*src >= '0' && *src <= '9') {
					token_val = token_val*10 + *src++ - '0';
				}
			} else {
				// starts with number 0
				if (*src == 'x' || *src == 'X') {
					//hex
					token = *++src;
					while ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F')) {
						token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
						token = *++src;
					}
				} else {
					// oct
					while (*src >= '0' && *src <= '7') {
						token_val = token_val*8 + *src++ - '0';
					}
				}
			}

			token = Num;
			return;
		}
		else if (token == '/') {
			if (*src == '/') {
				// skip comments
				while (*src != 0 && *src != '\n') {
					++src;
				}
			} else {
				// divide operator
				token = Div;
				return;
			}
		}
		else if (token == '"' || token == '\'') {
			// parse string literal, currently, the only supported escape
			// character is '\n', store the string literal into data.
			last_pos = data;
			while (*src != 0 && *src != token) {
				token_val = *src++;
				if (token_val == '\\') {
					// escape character
					token_val = *src++;
					if (token_val == 'n') { // newline
						token_val = '\n';
					}
					else if (token_val == '0') { // terminator
						token_val = '\0';
					}
					else if (token_val == 'r') { // carriage return
						token_val = '\r';
					}
					else if (token_val == 't') { // tab
						token_val = '\t';
					}
				}

				if (token == '"') {
					*data++ = token_val;
				}
			}

			src++;
			// if it is a single character, return Num token
			if (token == '"') {
				token_val = (int)last_pos;
			} else {
				token = Num;
			}

			return;
		}
		else if (token == '=') {
			// parse '==' and '='
			if (*src == '=') {
				src ++;
				token = Eq;
			} else {
				token = Assign;
			}
			return;
		}
		else if (token == '+') {
			// parse '+' and '++'
			if (*src == '+') {
				src ++;
				token = Inc;
			} else {
				token = Add;
			}
			return;
		}
		else if (token == '-') {
			// parse '-' and '--'
			if (*src == '-') {
				src ++;
				token = Dec;
			} else {
				token = Sub;
			}
			return;
		}
		else if (token == '!') {
			// parse '!='
			if (*src == '=') {
				src++;
				token = Ne;
			}
			return;
		}
		else if (token == '<') {
			// parse '<=', '<<' or '<'
			if (*src == '=') {
				src ++;
				token = Le;
			} else if (*src == '<') {
				src ++;
				token = Shl;
			} else {
				token = Lt;
			}
			return;
		}
		else if (token == '>') {
			// parse '>=', '>>' or '>'
			if (*src == '=') {
				src ++;
				token = Ge;
			} else if (*src == '>') {
				src ++;
				token = Shr;
			} else {
				token = Gt;
			}
			return;
		}
		else if (token == '|') {
			// parse '|' or '||'
			if (*src == '|') {
				src ++;
				token = Lor;
			} else {
				token = Or;
			}
			return;
		}
		else if (token == '&') {
			// parse '&' and '&&'
			if (*src == '&') {
				src ++;
				token = Lan;
			} else {
				token = And;
			}
			return;
		}
		else if (token == '^') {
			token = Xor;
			return;
		}
		else if (token == '%') {
			token = Mod;
			return;
		}
		else if (token == '*') {
			token = Mul;
			return;
		}
		else if (token == '[') {
			token = Brak;
			return;
		}
		else if (token == '?') {
			token = Cond;
			return;
		}
		else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':') {
			// directly return the character as token;
			return;
		}
	}
}

void match(int tk) {
	if (token == tk) {
		next();
	} else {
		errorinfo_line();
		die("[!]: expected token: %d", tk);
	}
}


void expression(int level) {
	// expressions have various format.
	// but majorly can be divided into two parts: unit and operator
	// for example `(char) *a[10] = (int *) func(b > 0 ? 10 : 20);
	// `a[10]` is an unit while `*` is an operator.
	// `func(...)` in total is an unit.
	// so we should first parse those unit and unary operators
	// and then the binary ones
	//
	// also the expression can be in the following types:
	//
	// 1. unit_unary ::= unit | unit unary_op | unary_op unit
	// 2. expr ::= unit_unary (bin_op unit_unary ...)

	// unit_unary()
	int *id;
	int tmp;
	int *addr;
	{
		if (!token) {
			errorinfo_line();
			die("[!]: unexpected token EOF of expression");
		}
		if (token == Num) {
			match(Num);

			// emit code
			*++text = IMM;
			*++text = token_val;
			expr_type = XC_INT;
		}
		else if (token == '"') {
			// continous string "abc" "abc"


			// emit code
			*++text = IMM;
			*++text = token_val;

			match('"');
			// store the rest strings
			while (token == '"') {
				match('"');
			}

			// append the end of string character '\0', all the data are default
			// to 0, so just move data one position forward.
			data = (char *)(((int)data + sizeof(int)) & (-sizeof(int)));
			expr_type = XC_PTR;
		}
		else if (token == Sizeof) {
			// sizeof is actually an unary operator
			// now only `sizeof(int)`, `sizeof(char)` and `sizeof(*...)` are
			// supported.
			match(Sizeof);
			match('(');
			expr_type = XC_INT;

			if (token == Int) {
				match(Int);
			} else if (token == Char) {
				match(Char);
				expr_type = XC_CHAR;
			}

			while (token == Mul) {
				match(Mul);
				expr_type = expr_type + XC_PTR;
			}

			match(')');

			// emit code
			*++text = IMM;
			*++text = (expr_type == XC_CHAR) ? sizeof(char) : sizeof(int);

			expr_type = XC_INT;
		}
		else if (token == Id) {
			// there are several type when occurs to Id
			// but this is unit, so it can only be
			// 1. function call
			// 2. Enum variable
			// 3. global/local variable
			match(Id);

			id = current_id;

			if (token == '(') {
				// function call
				match('(');

				// pass in arguments
				tmp = 0; // number of arguments
				while (token != ')') {
					expression(Assign);
					*++text = PUSH;
					tmp ++;

					if (token == ',') {
						match(',');
					}

				}
				match(')');

				// emit code
				if (id[Class] == Sys) {
					// system functions
					*++text = id[Value];
				}
				else if (id[Class] == Fun) {
					// function call
					*++text = CALL;
					*++text = id[Value];
				}
				else {
					errorinfo_line();
					die("[!]: bad function call");
				}

				// clean the stack for arguments
				if (tmp > 0) {
					*++text = ADJ;
					*++text = tmp;
				}
				expr_type = id[Type];
			}
			else if (id[Class] == Num) {
				// enum variable
				*++text = IMM;
				*++text = id[Value];
				expr_type = XC_INT;
			}
			else {
				// variable
				if (id[Class] == Loc) {
					*++text = LEA;
					*++text = index_of_bp - id[Value];
				}
				else if (id[Class] == Glo) {
					*++text = IMM;
					*++text = id[Value];
				}
				else {
					errorinfo_line();
					die("[!]: undefined variable");
				}

				// emit code, default behaviour is to load the value of the
				// address which is stored in `ax`
				expr_type = id[Type];
				*++text = (expr_type == XC_CHAR) ? LC : LI;
			}
		}
		else if (token == '(') {
			// cast or parenthesis
			match('(');
			if (token == Int || token == Char) {
				tmp = (token == Char) ? XC_CHAR : XC_INT; // cast type
				match(token);
				while (token == Mul) {
					match(Mul);
					tmp = tmp + XC_PTR;
				}

				match(')');

				expression(Inc); // cast has precedence as Inc(++)

				expr_type  = tmp;
			} else {
				// normal parenthesis
				expression(Assign);
				match(')');
			}
		}
		else if (token == Mul) {
			// dereference *<addr>
			match(Mul);
			expression(Inc); // dereference has the same precedence as Inc(++)

			if (expr_type >= XC_PTR) {
				expr_type = expr_type - XC_PTR;
			} else {
				errorinfo_line();
				die("[!]: bad dereference");
			}

			*++text = (expr_type == XC_CHAR) ? LC : LI;
		}
		else if (token == And) {
			// get the address of
			match(And);
			expression(Inc); // get the address of
			if (*text == LC || *text == LI) {
				text --;
			} else {
				errorinfo_line();
				die("[!]: bad address-of");
			}

			expr_type = expr_type + XC_PTR;
		}
		else if (token == '!') {
			// not
			match('!');
			expression(Inc);

			// emit code, use <expr> == 0
			*++text = PUSH;
			*++text = IMM;
			*++text = 0;
			*++text = EQ;

			expr_type = XC_INT;
		}
		else if (token == '~') {
			// bitwise not
			match('~');
			expression(Inc);

			// emit code, use <expr> XOR -1
			*++text = PUSH;
			*++text = IMM;
			*++text = -1;
			*++text = XOR;

			expr_type = XC_INT;
		}
		else if (token == Add) {
			// +var, do nothing
			match(Add);
			expression(Inc);

			expr_type = XC_INT;
		}
		else if (token == Sub) {
			// -var
			match(Sub);

			if (token == Num) {
				*++text = IMM;
				*++text = -token_val;
				match(Num);
			} else {

				*++text = IMM;
				*++text = -1;
				*++text = PUSH;
				expression(Inc);
				*++text = MUL;
			}

			expr_type = XC_INT;
		}
		else if (token == Inc || token == Dec) {
			tmp = token;
			match(token);
			expression(Inc);
			if (*text == LC) {
				*text = PUSH;  // to duplicate the address
				*++text = LC;
			} else if (*text == LI) {
				*text = PUSH;
				*++text = LI;
			} else {
				errorinfo_line();
				die("[!]: bad lvalue of pre-increment");
			}
			*++text = PUSH;
			*++text = IMM;
			*++text = (expr_type > XC_PTR) ? sizeof(int) : sizeof(char);
			*++text = (tmp == Inc) ? ADD : SUB;
			*++text = (expr_type == XC_CHAR) ? SC : SI;
		}
		else {
			errorinfo_line();
			die("[!]: bad expression");
		}
	}

	// binary operator and postfix operators.
	{
		while (token >= level) {
			// handle according to current operator's precedence
			tmp = expr_type;
			if (token == Assign) {
				// var = expr;
				match(Assign);
				if (*text == LC || *text == LI) {
					*text = PUSH; // save the lvalue's pointer
				} else {
					errorinfo_line();
					die("[!]: bad lvalue in assignment");
				}
				expression(Assign);

				expr_type = tmp;
				*++text = (expr_type == XC_CHAR) ? SC : SI;
			}
			else if (token == Cond) {
				// expr ? a : b;
				match(Cond);
				*++text = JZ;
				addr = ++text;
				expression(Assign);
				if (token == ':') {
					match(':');
				} else {
					errorinfo_line();
					die("[!]: missing colon in conditional");
				}
				*addr = (int)(text + 3);
				*++text = JMP;
				addr = ++text;
				expression(Cond);
				*addr = (int)(text + 1);
			}
			else if (token == Lor) {
				// logic or
				match(Lor);
				*++text = JNZ;
				addr = ++text;
				expression(Lan);
				*addr = (int)(text + 1);
				expr_type = XC_INT;
			}
			else if (token == Lan) {
				// logic and
				match(Lan);
				*++text = JZ;
				addr = ++text;
				expression(Or);
				*addr = (int)(text + 1);
				expr_type = XC_INT;
			}
			else if (token == Or) {
				// bitwise or
				match(Or);
				*++text = PUSH;
				expression(Xor);
				*++text = OR;
				expr_type = XC_INT;
			}
			else if (token == Xor) {
				// bitwise xor
				match(Xor);
				*++text = PUSH;
				expression(And);
				*++text = XOR;
				expr_type = XC_INT;
			}
			else if (token == And) {
				// bitwise and
				match(And);
				*++text = PUSH;
				expression(Eq);
				*++text = AND;
				expr_type = XC_INT;
			}
			else if (token == Eq) {
				// equal ==
				match(Eq);
				*++text = PUSH;
				expression(Ne);
				*++text = EQ;
				expr_type = XC_INT;
			}
			else if (token == Ne) {
				// not equal !=
				match(Ne);
				*++text = PUSH;
				expression(Lt);
				*++text = NE;
				expr_type = XC_INT;
			}
			else if (token == Lt) {
				// less than
				match(Lt);
				*++text = PUSH;
				expression(Shl);
				*++text = LT;
				expr_type = XC_INT;
			}
			else if (token == Gt) {
				// greater than
				match(Gt);
				*++text = PUSH;
				expression(Shl);
				*++text = GT;
				expr_type = XC_INT;
			}
			else if (token == Le) {
				// less than or equal to
				match(Le);
				*++text = PUSH;
				expression(Shl);
				*++text = LE;
				expr_type = XC_INT;
			}
			else if (token == Ge) {
				// greater than or equal to
				match(Ge);
				*++text = PUSH;
				expression(Shl);
				*++text = GE;
				expr_type = XC_INT;
			}
			else if (token == Shl) {
				// shift left
				match(Shl);
				*++text = PUSH;
				expression(Add);
				*++text = SHL;
				expr_type = XC_INT;
			}
			else if (token == Shr) {
				// shift right
				match(Shr);
				*++text = PUSH;
				expression(Add);
				*++text = SHR;
				expr_type = XC_INT;
			}
			else if (token == Add) {
				// add
				match(Add);
				*++text = PUSH;
				expression(Mul);

				expr_type = tmp;
				if (expr_type > XC_PTR) {
					// pointer type, and not `char *`
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = MUL;
				}
				*++text = ADD;
			}
			else if (token == Sub) {
				// sub
				match(Sub);
				*++text = PUSH;
				expression(Mul);
				if (tmp > XC_PTR && tmp == expr_type) {
					// pointer subtraction
					*++text = SUB;
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = DIV;
					expr_type = XC_INT;
				} else if (tmp > XC_PTR) {
					// pointer movement
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = MUL;
					*++text = SUB;
					expr_type = tmp;
				} else {
					// numeral subtraction
					*++text = SUB;
					expr_type = tmp;
				}
			}
			else if (token == Mul) {
				// multiply
				match(Mul);
				*++text = PUSH;
				expression(Inc);
				*++text = MUL;
				expr_type = tmp;
			}
			else if (token == Div) {
				// divide
				match(Div);
				*++text = PUSH;
				expression(Inc);
				*++text = DIV;
				expr_type = tmp;
			}
			else if (token == Mod) {
				// Modulo
				match(Mod);
				*++text = PUSH;
				expression(Inc);
				*++text = MOD;
				expr_type = tmp;
			}
			else if (token == Inc || token == Dec) {
				// postfix inc(++) and dec(--)
				// we will increase the value to the variable and decrease it
				// on `ax` to get its original value.
				if (*text == LI) {
					*text = PUSH;
					*++text = LI;
				}
				else if (*text == LC) {
					*text = PUSH;
					*++text = LC;
				}
				else {
					errorinfo_line();
					die("[!]: bad value in increment");
				}

				*++text = PUSH;
				*++text = IMM;
				*++text = (expr_type > XC_PTR) ? sizeof(int) : sizeof(char);
				*++text = (token == Inc) ? ADD : SUB;
				*++text = (expr_type == XC_CHAR) ? SC : SI;
				*++text = PUSH;
				*++text = IMM;
				*++text = (expr_type > XC_PTR) ? sizeof(int) : sizeof(char);
				*++text = (token == Inc) ? SUB : ADD;
				match(token);
			}
			else if (token == Brak) {
				// array access var[xx]
				match(Brak);
				*++text = PUSH;
				expression(Assign);
				match(']');

				if (tmp > XC_PTR) {
					// pointer, `not char *`
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = MUL;
				}
				else if (tmp < XC_PTR) {
					errorinfo_line();
					die("[!]: pointer type expected");
				}
				expr_type = tmp - XC_PTR;
				*++text = ADD;
				*++text = (expr_type == XC_CHAR) ? LC : LI;
			}
			else {
				errorinfo_line();
				die("[!]: compiler error, token = %d", token);
			}
		}
	}
}

void statement() {
	// there are 8 kinds of statements here:
	// 1. if (...) <statement> [else <statement>]
	// 2. while (...) <statement>
	// 3. { <statement> }
	// 4. return xxx;
	// 5. <empty statement>;
	// 6. expression; (expression end with semicolon)

	int *a, *b; // bess for branch control

	if (token == If) {
		// if (...) <statement> [else <statement>]
		//
		//   if (...)           <cond>
		//                      JZ a
		//     <statement>      <statement>
		//   else:              JMP b
		// a:
		//     <statement>      <statement>
		// b:                   b:
		//
		//
		match(If);
		match('(');
		expression(Assign);  // parse condition
		match(')');

		// emit code for if
		*++text = JZ;
		b = ++text;

		statement();         // parse statement
		if (token == Else) { // parse else
			match(Else);

			// emit code for JMP B
			*b = (int)(text + 3);
			*++text = JMP;
			b = ++text;

			statement();
		}

		*b = (int)(text + 1);
	}
	else if (token == While) {
		//
		// a:                     a:
		//    while (<cond>)        <cond>
		//                          JZ b
		//     <statement>          <statement>
		//                          JMP a
		// b:                     b:
		match(While);

		a = text + 1;

		match('(');
		expression(Assign);
		match(')');

		*++text = JZ;
		b = ++text;

		statement();

		*++text = JMP;
		*++text = (int)a;
		*b = (int)(text + 1);
	}
	else if (token == '{') {
		// { <statement> ... }
		match('{');

		while (token != '}') {
			statement();
		}

		match('}');
	}
	else if (token == Return) {
		// return [expression];
		match(Return);

		if (token != ';') {
			expression(Assign);
		}

		match(';');

		// emit code for return
		*++text = LEV;
	}
	else if (token == ';') {
		// empty statement
		match(';');
	}
	else {
		// a = b; or function_call();
		expression(Assign);
		match(';');
	}
}

void enum_declaration() {
	// parse enum [id] { a = 1, b = 3, ...}
	int i;
	i = 0;
	while (token != '}') {
		if (token != Id) {
			errorinfo_line();
			die("[!]: bad enum identifier %d", token);
		}
		next();
		if (token == Assign) {
			// like {a=10}
			next();
			if (token != Num) {
				errorinfo_line();
				die("[!]: bad enum initializer");
			}
			i = token_val;
			next();
		}

		current_id[Class] = Num;
		current_id[Type] = XC_INT;
		current_id[Value] = i++;

		if (token == ',') {
			next();
		}
	}
}

void function_parameter() {
	int type;
	int params;
	params = 0;
	while (token != ')') {
		// int name, ...
		type = XC_INT;
		if (token == Int) {
			match(Int);
		} else if (token == Char) {
			type = XC_CHAR;
			match(Char);
		}

		// pointer type
		while (token == Mul) {
			match(Mul);
			type = type + XC_PTR;
		}

		// parameter name
		if (token != Id) {
			errorinfo_line();
			die("[!]: bad parameter declaration");
		}
		if (current_id[Class] == Loc) {
			errorinfo_line();
			die("[!]: duplicate parameter declaration");
		}

		match(Id);
		// store the local variable
		current_id[BClass] = current_id[Class]; current_id[Class]  = Loc;
		current_id[BType]  = current_id[Type];  current_id[Type]   = type;
		current_id[BValue] = current_id[Value]; current_id[Value]  = params++;   // index of current parameter

		if (token == ',') {
			match(',');
		}
	}
	index_of_bp = params+1;
}

void function_body() {
	// type func_name (...) {...}
	//                   -->|   |<--

	// ... {
	// 1. local declarations
	// 2. statements
	// }

	int pos_local; // position of local variables on the stack.
	int type;
	pos_local = index_of_bp;

	while (token == Int || token == Char) {
		// local variable declaration, just like global ones.
		basetype = (token == Int) ? XC_INT : XC_CHAR;
		match(token);

		while (token != ';') {
			type = basetype;
			while (token == Mul) {
				match(Mul);
				type = type + XC_PTR;
			}

			if (token != Id) {
				// invalid declaration
				errorinfo_line();
				die("[!]: bad local declaration");
			}
			if (current_id[Class] == Loc) {
				// identifier exists
				errorinfo_line();
				die("[!]: duplicate local declaration");
			}
			match(Id);

			// store the local variable
			current_id[BClass] = current_id[Class]; current_id[Class]  = Loc;
			current_id[BType]  = current_id[Type];  current_id[Type]   = type;
			current_id[BValue] = current_id[Value]; current_id[Value]  = ++pos_local;   // index of current parameter

			if (token == ',') {
				match(',');
			}
		}
		match(';');
	}

	// save the stack size for local variables
	*++text = ENT;
	*++text = pos_local - index_of_bp;

	// statements
	while (token != '}') {
		statement();
	}

	// emit code for leaving the sub function
	*++text = LEV;
}

void function_declaration() {
	// type func_name (...) {...}
	//               | this part

	match('(');
	function_parameter();
	match(')');
	match('{');
	function_body();
	//match('}');

	// unwind local variable declarations for all local variables.
	current_id = symbols;
	while (current_id[Token]) {
		if (current_id[Class] == Loc) {
			current_id[Class] = current_id[BClass];
			current_id[Type]  = current_id[BType];
			current_id[Value] = current_id[BValue];
		}
		current_id = current_id + IdSize;
	}
}

void global_declaration() {
	// int [*]id [; | (...) {...}]


	int type; // tmp, actual type for variable
	int i; // tmp

	basetype = XC_INT;

	// parse enum, this should be treated alone.
	if (token == Enum) {
		// enum [id] { a = 10, b = 20, ... }
		match(Enum);
		if (token != '{') {
			match(Id); // skip the [id] part
		}
		if (token == '{') {
			// parse the assign part
			match('{');
			enum_declaration();
			match('}');
		}

		match(';');
		return;
	}

	// parse type information
	if (token == Int) {
		match(Int);
	}
	else if (token == Char) {
		match(Char);
		basetype = XC_CHAR;
	}

	// parse the comma seperated variable declaration.
	while (token != ';' && token != '}') {
		type = basetype;
		// parse pointer type, note that there may exist `int ****x;`
		while (token == Mul) {
			match(Mul);
			type = type + XC_PTR;
		}

		if (token != Id) {
			// invalid declaration
			errorinfo_line();
			die("[!]: bad global declaration");
		}
		if (current_id[Class]) {
			// identifier exists
			errorinfo_line();
			die("[!]: duplicate global declaration");
		}
		match(Id);
		current_id[Type] = type;

		if (token == '(') {
			current_id[Class] = Fun;
			current_id[Value] = (int)(text + 1); // the memory address of function
			function_declaration();
		} else {
			// variable declaration
			current_id[Class] = Glo; // global variable
			current_id[Value] = (int)data; // assign memory address
			data = data + sizeof(int);
		}

		if (token == ',') {
			match(',');
		}
	}
	next();
}

void program() {
	// get next token
	next();
	while (token > 0) {
		global_declaration();
	}
}

#if 0
struct rom {
	int value;
};

void *rom_new(int value)
{
	struct rom *rom;
	rom = malloc_safe(sizeof(*rom));
	rom->value = value;
	return rom;
}

void rom_test(struct rom *rom)
{
	fprintf(stderr, "value = %d\n", rom->value);
}
#endif

int eval() {
	int op, *tmp;
	cycle = 0;
	while (1) {
		cycle ++;
		op = *pc++; // get next operation code

		// print debug info
#ifndef NDEBUG
		if (debug) {
			printf("%d> %.4s", cycle,
				   & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
"OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
"OPEN,READ,CLOS,FPRT,SPRT,SSCF,PRTF,MALC,FREE,MSET,MCMP,MCPY,SSTM,SCAT,SCMP,SCCM,EXIT"
ZZXC_FUNC_INST_STR[op * 5]
);
			if (op <= ADJ)
				printf(" %d\n", *pc);
			else
				printf("\n");
		}
#endif
		if (op == IMM)       {ax = *pc++;}                                     // load immediate value to ax
		else if (op == LC)   {ax = *(char *)ax;}                               // load character to ax, address in ax
		else if (op == LI)   {ax = *(int *)ax;}                                // load integer to ax, address in ax
		else if (op == SC)   {ax = *(char *)*sp++ = ax;}                       // save character to address, value in ax, address on stack
		else if (op == SI)   {*(int *)*sp++ = ax;}                             // save integer to address, value in ax, address on stack
		else if (op == PUSH) {*--sp = ax;}                                     // push the value of ax onto the stack
		else if (op == JMP)  {pc = (int *)*pc;}                                // jump to the address
		else if (op == JZ)   {pc = ax ? pc + 1 : (int *)*pc;}                   // jump if ax is zero
		else if (op == JNZ)  {pc = ax ? (int *)*pc : pc + 1;}                   // jump if ax is not zero
		else if (op == CALL) {*--sp = (int)(pc+1); pc = (int *)*pc;}           // call subroutine
		//else if (op == RET)  {pc = (int *)*sp++;}                              // return from subroutine;
		else if (op == ENT)  {*--sp = (int)bp; bp = sp; sp = sp - *pc++;}      // make new stack frame
		else if (op == ADJ)  {sp = sp + *pc++;}                                // add esp, <size>
		else if (op == LEV)  {sp = bp; bp = (int *)*sp++; pc = (int *)*sp++;}  // restore call frame and PC
		else if (op == LEA)  {ax = (int)(bp + *pc++);}                         // load address for arguments.

		else if (op == OR)  ax = *sp++ | ax;
		else if (op == XOR) ax = *sp++ ^ ax;
		else if (op == AND) ax = *sp++ & ax;
		else if (op == EQ)  ax = *sp++ == ax;
		else if (op == NE)  ax = *sp++ != ax;
		else if (op == LT)  ax = *sp++ < ax;
		else if (op == LE)  ax = *sp++ <= ax;
		else if (op == GT)  ax = *sp++ >  ax;
		else if (op == GE)  ax = *sp++ >= ax;
		else if (op == SHL) ax = *sp++ << ax;
		else if (op == SHR) ax = *sp++ >> ax;
		else if (op == ADD) ax = *sp++ + ax;
		else if (op == SUB) ax = *sp++ - ax;
		else if (op == MUL) ax = *sp++ * ax;
		else if (op == DIV) ax = *sp++ / ax;
		else if (op == MOD) ax = *sp++ % ax;

		else if (op == EXIT) { /*printf("exit(%d)", *sp);*/ return *sp;}
		else if (op == OPEN) {
			ax = (int)fopen((char *)sp[1], (char*)sp[0]);
			if (!ax)
				die("[!] failed to open '%s' for writing", (char*)sp[1]);
		}
		else if (op == CLOS) {
			if (!*sp)
				die("[!] trying to fclose() FILE not yet fopen()'d");
			ax = fclose((FILE*)*sp);
		}
		else if (op == FPRT) {
			tmp = sp + pc[1];
			if (!tmp[-1])
				die("[!] trying to fprintf() to FILE not yet fopen()'d");
			ax =
			fprintf(
				(FILE*)tmp[-1]
				, (char *)tmp[-2]
				, tmp[-3], tmp[-4], tmp[-5], tmp[-6]
			);
		}
		else if (op == SPRT) {
			tmp = sp + pc[1];
			if (!tmp[-1])
				die("[!] trying to sprintf() into invalid pointer");
			ax =
			sprintf(
				(char*)tmp[-1]
				, (char *)tmp[-2]
				, tmp[-3], tmp[-4], tmp[-5], tmp[-6]
			);
		}
		else if (op == SSCF) {
			tmp = sp + pc[1];
			if (!tmp[-1])
				die("[!] trying to sscanf() from invalid pointer");
			ax =
			sscanf(
				(char*)tmp[-1]
				, (char *)tmp[-2]
				, tmp[-3], tmp[-4], tmp[-5], tmp[-6]
			);
		}
		//else if (op == READ) { ax = read(sp[2], (char *)sp[1], *sp); }
		else if (op == PRTF) { tmp = sp + pc[1]; ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); fflush(stdout); }
		else if (op == MALC) { ax = (int)malloc_safe(*sp);}
		else if (op == FREE) { if (*sp) free((void*)*sp);}
		else if (op == MSET) {
			if (!sp[2])
				die("[!] memset() invalid pointer");
			ax = (int)memset((char *)sp[2], sp[1], *sp);
		}
		else if (op == MCMP) {
			if (!sp[2] || !sp[1])
				die("[!] memcmp() invalid pointer");
			ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
		}
		else if (op == MCPY) {
			if (!sp[1] || !sp[2])
				die("[!] memcpy() invalid pointer");
			ax = (int)memcpy((void*)sp[2], (void*)sp[1], *sp);
		}
		else if (op == SCMP) {
			if (!(char*)*sp || !(char*)sp[1])
				die("[!] strcmp() invalid pointer");
			ax = strcmp((char *)sp[1], (char*)*sp);
		}
		else if (op == SCCM) {
			if (!(char*)*sp || !(char*)sp[1])
				die("[!] strcasecmp() invalid pointer");
			ax = strcasecmp((char *)sp[1], (char*)*sp);
		}
		else if (op == SSTM)
		{
			if (!(char*)*sp)
				die("[!] system() invalid pointer");
			ax = wow_system((char*)*sp);
			/* revision 4 (r4): support wide system chars */
		}
		else if (op == SCAT)
		{
			if (!(char*)*sp || !(char*)sp[1])
				die("[!] strcat() invalid pointer");
			ax = (int)strcat((char*)sp[1], (char*)*sp);
		}
		
		/* extended stuff */
		/* rom */
		else if (op == ZZXC_ROM_NEW)
			ax = (int)rom_new((struct rom*)sp[1], (char*)*sp);
		
		else if (op == ZZXC_ROM_FREE)
			rom_free((struct rom*)*sp);
		
		else if (op == ZZCX_ROM_RAW)
			ax = (int)rom_raw((struct rom*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_SAVE)
			rom_save((struct rom*)sp[1], (char*)*sp);
		
		else if (op == ZZXC_ROM_ALIGN)
			rom_align((struct rom*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_COMPRESS)
			rom_compress(
				(struct rom*)sp[2]
				, (char*)sp[1]
				, *sp
			);
		
		else if (op == ZZXC_ROM_SEEK)
			rom_seek((struct rom*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_SEEK_CUR)
			rom_seek_cur((struct rom*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_TELL)
			ax = rom_tell((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_SIZE)
			ax = rom_size((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_INJECT)
			ax = (int)rom_inject(
				(struct rom*)sp[2]  /* rom   */
				, (char*)sp[1]      /* fn    */
				, *sp               /* comp  */
			);
		
		else if (op == ZZXC_ROM_EXTRACT)
			rom_extract((struct rom*)sp[3], (char*)sp[2], sp[1], *sp);
		
		else if (op == ZZXC_ROM_FILE_START)
			ax = rom_file_start((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_FILE_END)
			ax = rom_file_end((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_FILE_SZ)
			ax = rom_file_sz((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_FILE_DMA)
			ax = rom_file_dma((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_FILE)
			ax = (int)rom_file((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_INJECT_PNG)
			ax = (int)rom_inject_png(
				(struct rom*)sp[4]  /* rom   */
				, (char*)sp[3]      /* fn    */
				, sp[2]             /* fmt   */
				, sp[1]             /* bpp   */
				, *sp               /* comp  */
			);
		
		else if (op == ZZXC_ROM_INJECT_RAW)
			ax = (int)rom_inject_raw(
				(struct rom*)sp[3]  /* rom   */
				, (void*)sp[2]      /* raw   */
				, sp[1]             /* sz    */
				, *sp               /* comp  */
			);
		
		else if (op == ZZXC_ROM_EXTRACT_PNG)
			rom_extract_png(
				(struct rom*)sp[8]  /* rom   */
				, (char*)sp[7]      /* fn    */
				, (void*)sp[6]      /* buf   */
				, sp[5]             /* tex   */
				, sp[4]             /* pal   */
				, sp[3]             /* w     */
				, sp[2]             /* h     */
				, sp[1]             /* fmt   */
				, *sp               /* bpp   */
			);
		
		else if (op == ZZXC_ROM_WRITE)
			rom_write((struct rom*)sp[2], (unsigned char*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_WRITE32)
			rom_write32((struct rom*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_WRITE24)
			rom_write24((struct rom*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_WRITE16)
			rom_write16((struct rom*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_WRITE8)
			rom_write8((struct rom*)sp[1], *sp);
		
		else if (op == ZZXC_ROM_READ32)
			ax = rom_read32((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_READ24)
			ax = rom_read24((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_READ16)
			ax = rom_read16((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_READ8)
			ax = rom_read8((struct rom*)*sp);
		
		else if (op == ZZXC_ROM_DMA)
			rom_dma(
				(struct rom*)sp[2]  /* rom   */
				, sp[1]             /* ofs   */
				, *sp               /* num   */
			);
		
		else if (op == ZZXC_ROM_DMA_QUEUE_ONE)
			rom_dma_queue_one(
				(struct rom*)sp[1]  /* rom   */
				, sp[0]             /* idx   */
			);
		
		else if (op == ZZXC_ROM_DMA_QUEUE)
			rom_dma_queue(
				(struct rom*)sp[2]  /* rom   */
				, sp[1]             /* start */
				, sp[0]             /* end   */
			);
		
		else if (op == ZZXC_ROM_DMA_COMPRESS_ONE)
			rom_dma_compress_one(
				(struct rom*)sp[2]  /* rom   */
				, sp[1]             /* idx   */
				, *sp               /* comp  */
			);
		
		else if (op == ZZXC_ROM_DMA_COMPRESS)
			rom_dma_compress(
				(struct rom*)sp[3]  /* rom   */
				, sp[2]             /* start */
				, sp[1]             /* end   */
				, *sp               /* comp  */
			);
		
		else if (op == ZZXC_ROM_DMA_REARCHIVE_ONE)
			rom_dma_rearchive_one(
				(struct rom*)sp[4]  /* rom   */
				, sp[3]             /* idx   */
				, (char*)sp[2]      /* from  */
				, (char*)sp[1]      /* to    */
				, sp[0]             /* reloc */
			);
		
		else if (op == ZZXC_ROM_DMA_REARCHIVE)
			rom_dma_rearchive(
				(struct rom*)sp[5]  /* rom   */
				, sp[4]             /* start */
				, sp[3]             /* end   */
				, (char*)sp[2]      /* from  */
				, (char*)sp[1]      /* to    */
				, sp[0]             /* reloc */
			);
		
		else if (op == ZZXC_ROM_DMA_READY)
			rom_dma_ready((struct rom*)*sp);
		
		else if (op == ZZXC_rom_dma_register)
			rom_dma_register(
				(struct rom*)sp[1]  /* rom   */
				, *sp               /* ofs   */
			);
		
		else if (op == ZZXC_rom_dma_prefix)
			ax = (int)rom_dma_prefix(
				(struct rom*)sp[0]  /* rom   */
			);
		
		else if (op == ZZXC_rom_inject_prefix)
			ax = (int)rom_inject_prefix(
				(struct rom*)sp[0]  /* rom   */
				//(struct rom*)sp[1]  /* rom   */
				//, (void*)sp[0]      /* prfx  */
			);
		
		else if (op == ZZXC_rom_inject_mode)
			rom_inject_mode(
				(struct rom*)sp[1]  /* rom   */
				, (char*)sp[0]      /* mode  */
			);
		
		else if (op == ZZXC_rom_dma_offset)
			ax = (int)rom_dma_offset(
				(struct rom*)sp[1]  /* rom   */
				, *sp               /* idx   */
			);
		
		else if (op == ZZXC_ROM_CLOUDPATCH)
			rom_cloudpatch(
				(struct rom*)sp[2]  /* rom   */
				, sp[1]             /* ofs   */
				, (char*)sp[0]      /* fn    */
			);
		
		else if (op == ZZXC_rom_inject_raw_dma)
			rom_inject_raw_dma(
				(struct rom*)sp[4]  /* rom   */
				, (void*)sp[3]      /* raw   */
				, sp[2]             /* sz    */
				, sp[1]             /* comp  */
				, sp[0]             /* idx   */
			);
		
		else if (op == ZZXC_rom_inject_dma)
			rom_inject_dma(
				(struct rom*)sp[3]  /* rom   */
				, (char*)sp[2]      /* fn    */
				, sp[1]             /* comp  */
				, sp[0]             /* idx   */
			);
		
		else if (op == ZZXC_rom_extract_dma)
			rom_extract_dma(
				(struct rom*)sp[2]  /* rom   */
				, (char*)sp[1]      /* fn    */
				, sp[0]             /* idx   */
			);
		
		else if (op == ZZXC_rom_filename)
			ax = (int)rom_filename(
				(struct rom*)sp[0]  /* rom   */
			);
		
		else if (op == ZZXC_rom_flag)
			rom_flag(
				(struct rom*)sp[2]  /* rom   */
				, (char*)sp[1]      /* flag  */
				, sp[0]             /* on    */
			);
#if 0
		else if (op == ZZXC_ROM_WRITABLE)
			rom_writable(
				(struct rom*)sp[2]  /* rom   */
				, sp[1]             /* start */
				, *sp               /* end   */
			);
#endif
	/* generic */
		else if (op == ZZXC_DIE) {
			tmp = sp + pc[1];
			die(
				(char *)tmp[-1], tmp[-2], tmp[-3]
				, tmp[-4], tmp[-5], tmp[-6]
			);
		}
		else if (op == ZZXC_GET32)
			ax = get32((unsigned char*)*sp);
		
		else if (op == ZZXC_GET24)
			ax = get24((unsigned char*)*sp);
		
		else if (op == ZZXC_GET16)
			ax = get16((unsigned char*)*sp);
		
		else if (op == ZZXC_GET8)
			ax = get8((unsigned char*)*sp);
		
		else if (op == ZZXC_PUT32)
			put32((void*)sp[1], *sp);
		
		else if (op == ZZXC_PUT24)
			put24((void*)sp[1], *sp);
		
		else if (op == ZZXC_PUT16)
			put16((void*)sp[1], *sp);
		
		else if (op == ZZXC_PUT8)
			put8((void*)sp[1], *sp);
		
		else if (op == ZZXC_U8)
			ax = u8(*sp);
		
		else if (op == ZZXC_S8)
			ax = s8(*sp);
		
		else if (op == ZZXC_U16)
			ax = u16(*sp);
		
		else if (op == ZZXC_S16)
			ax = s16(*sp);
		
		else if (op == ZZXC_U32OP)
			ax = u32op(sp[2], (char*)sp[1], *sp);
		
		else if (op == ZZXC_SUBSTRING)
			ax = (int)substring((char*)sp[1], *sp);
		
		else if (op == ZZXC_FIND_BYTES)
			ax =
			(int)find_bytes(
				(void*)sp[3]       /* hay    */
				, sp[2]            /* hay_sz */
				, (char*)sp[1]     /* ndl    */
				, *sp              /* only1  */
			);
		
		else if (op == ZZXC_FIND_BYTES_STRIDE)
			ax =
			(int)find_bytes_stride(
				(void*)sp[4]       /* hay    */
				, sp[3]            /* hay_sz */
				, (char*)sp[2]     /* ndl    */
				, sp[1]            /* stride */
				, *sp              /* only1  */
			);
		
		else if (op == ZZXC_FIND_TEXT)
			ax =
			(int)find_text(
				(void*)sp[3]       /* hay    */
				, sp[2]            /* hay_sz */
				, (char*)sp[1]     /* ndl    */
				, *sp              /* only1  */
			);
		
		else if (op == ZZXC_FIND_TEXT_STRIDE)
			ax =
			(int)find_text_stride(
				(void*)sp[4]       /* hay    */
				, sp[3]            /* hay_sz */
				, (char*)sp[2]     /* ndl    */
				, sp[1]            /* stride */
				, *sp              /* only1  */
			);
		
		else if (op == ZZXC_OVL_VRAM_SZ)
			ax = (int)ovl_vram_sz((void*)sp[1], *sp);
		
		else if (op == ZZXC_LOAD_PNG)
			ax =
			(int)load_png(
				(char*)sp[2]       /* fn     */
				, (REGXC_INT*)sp[1]   /* w      */
				, (REGXC_INT*)*sp     /* h      */
			);
		
		else if (op == ZZXC_int_array)
		{
			int *arr;
			int *Oarr;
			
			tmp = sp + pc[1];
			if (tmp[-1] <= 0)
				die("[!] int_array() invalid count");
			Oarr = malloc_safe(tmp[-1] * sizeof(*Oarr));
			ax = (int)Oarr;
			/* [0] = [-2], [1] = [-3], etc */
			for (arr = Oarr; arr - Oarr < tmp[-1]; ++arr)
				arr[0] = tmp[-((arr - Oarr)+2)];
		}
		
		else if (op == ZZXC_new_string)
		{
			char *str;
			size_t str_sz = 0;
			int i;
			
			tmp = sp + pc[1];
			if (tmp[-1] == 0)
				die("[!] new_string() invalid list");
			
			/* [0] = [-1], [1] = [-2], etc */
			for (i = 0; tmp[-(i+1)]; ++i)
				str_sz += strlen((char *)tmp[-(i+1)]) + 1;
			
			str = malloc_safe(str_sz);
			str[0] = '\0';
			for (i = 0; tmp[-(i+1)]; ++i)
				strcat(str, (char *)tmp[-(i+1)]);
			
			ax = (int)str;
		}
		
		/* string list from file */
		else if (op == ZZXC_string_list_file)
		{
			/* TODO */
		}
		
		/* load a file */
		else if (op == ZZXC_loadfile)
		{
			ax = (int)loadfile((char *)sp[2], (REGXC_INT *)sp[1], *sp);
		}
		
		/* extract string from tsv */
		else if (op == ZZXC_tsv_col_row)
		{
			ax = (int)tsv_col_row((void *)sp[2], (char *)sp[1], *sp);
		}
	
	/* directory */
		else if (op == ZZXC_DIR_EXISTS)
			ax = dir_exists((char*)*sp);
		
		else if (op == ZZXC_DIR_ENTER)
			dir_enter((char*)*sp);
		
		else if (op == ZZXC_DIR_ENTER_FILE)
			dir_enter_file((char*)*sp);
		
		else if (op == ZZXC_DIR_LEAVE)
			dir_leave();
		
		else if (op == ZZXC_DIR_MKNAME)
			ax = (int)dir_mkname(sp[1], (char*)*sp);
		
		else if (op == ZZXC_DIR_USE_STYLE)
			dir_use_style((char*)*sp);
		
		else if (op == ZZXC_FILE_EXISTS)
			ax = file_exists((char*)*sp);
		
	/* folder */
		else if (op == ZZXC_FOLDER_NEW)
			ax = (int)folder_new(
				(struct folder*)sp[1]  /* folder */
				, (char*)*sp           /* ext    */
			);
		
		else if (op == ZZXC_FOLDER_FREE)
			folder_free((struct folder*)*sp);
		
		else if (op == ZZXC_FOLDER_NAME)
			ax = (int)folder_name((struct folder*)*sp);
		
		else if (op == ZZXC_FOLDER_INDEX)
			ax = (int)folder_index((struct folder*)*sp);
		
		else if (op == ZZXC_FOLDER_NEXT)
			ax = (int)folder_next((struct folder*)*sp);
		
		else if (op == ZZXC_FOLDER_COUNT)
			ax = (int)folder_count((struct folder*)*sp);
		
		else if (op == ZZXC_FOLDER_REMAINING)
			ax = (int)folder_remaining((struct folder*)*sp);
		
		else if (op == ZZXC_FOLDER_MAX)
			ax = (int)folder_max((struct folder*)*sp);
	
	/* conf */
		else if (op == ZZXC_CONF_NEW)
			ax = (int)conf_new(
				(struct conf*)sp[2]    /* conf   */
				, (char*)sp[1]         /* fn     */
				, (char*)*sp           /* fmt    */
			);
		
		else if (op == ZZXC_CONF_FREE)
			conf_free((struct conf*)*sp);
		
		else if (op == ZZXC_CONF_GET_XC_INT)
			ax = conf_get_int(
				(struct conf*)sp[1]    /* conf   */
				, (char*)*sp           /* name   */
			);
		
		else if (op == ZZXC_CONF_GET_STR)
			ax = (int)conf_get_str(
				(struct conf*)sp[1]    /* conf   */
				, (char*)*sp           /* name   */
			);
		
		else if (op == ZZXC_CONF_NEXT)
			ax = conf_next((struct conf*)*sp);
		
		else if (op == ZZXC_CONF_REMAINING)
			ax = conf_remaining((struct conf*)*sp);
		
		else if (op == ZZXC_CONF_EXISTS)
			ax = conf_exists((struct conf*)sp[1], (char*)*sp);
		
		else if (op == ZZXC_CONF_NAME)
			ax = (int)conf_name((struct conf*)sp[0]);
		
		else if (op == ZZXC_CONF_VALUE)
			ax = (int)conf_value((struct conf*)sp[0]);
		
		/* failure */
		else {
			die("[!] unknown instruction:%d", op);
		}
	}
}

#if (_WIN32 && UNICODE)
int wmain(int argc, wchar_t *Wargv[])
{
	char **argv = wow_conv_args(argc, (void*)Wargv);
#else
REGXC_INT main(REGXC_INT argc, char *argv[])
{
#endif
	REGXC_INT Oargc = argc;
	REGXC_INT rval;
	REGXC_INT no_getchar = 0;
	REGXC_INT kb = 256;
	REGXC_INT do_show_args = 0;
	int i;
	int *tmp;
	size_t src_sz;
	FILE *fd;

	argc--;
	argv++;
	
	zzrtl();

	// parse arguments
	while (argc > 0 && (*argv)[0] == '-' && (*argv)[1] == '-')
	{
		char *ss;
		
		ss = (*argv)+2;
		
		/* set no_waiting = 1 to skip getchar() calls */
		if (!strcmp(ss, "nowaiting") || !strcmp(ss, "no_waiting"))
		{
			die_no_waiting(1);
			no_getchar = 1;
		}
		
		/* set use_cache = 0 to skip caching calls */
		else if (!strcmp(ss, "nocache") || !strcmp(ss, "no_cache"))
		{
			zzrtl_use_cache(0);
		}
		
		/* set kb=%d */
		else if (!memcmp(ss, "kb=", 3))
		{
			if (sscanf(ss+3, "%d", &kb) != 1)
				goto invalid_argument;
			if (kb <= 0)
				die("[!] invalid kb %d", kb);
		}
		
		/* jump to function showargs_cmd */
		else if (!strcmp(ss, "help"))
			showargs_cmd();
		
		/* jump to function main_compress */
		else if (!strcmp(ss, "compress"))
		{
			if (argc < 2)
				die("--compress must be followed by arguments");
			
			extern void main_compress(char *argstring);
			main_compress(argv[1]);
		}
		
		/* jump to function main_cloudpatch */
		else if (!strcmp(ss, "cloudpatch"))
		{
			if (argc < 4)
				die("--cloudpatch must be followed by arguments");
			
			extern void main_cloudpatch(REGXC_INT argc, char **argv);
			main_cloudpatch(argc, argv);
		}
		
		else
		{
invalid_argument:
			die("[!] unknown argument '%s'; try --help if you're lost\n", *argv);
			do_show_args = 1;
		}
		--argc;
		++argv;
	}
#if 0
	if (argc > 0 && **argv == '-' && (*argv)[1] == 's') {
		assembly = 1;
		--argc;
		++argv;
	}
	if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') {
		debug = 1;
		--argc;
		++argv;
	}
#endif
	if (argc <= 0 || do_show_args)
	{
		if (do_show_args)
			fprintf(stderr, "\n");
		showargs(Oargc);
	}
	
	/* not rtl extension */
	if (!is_ext(*argv, "rtl"))
		die("[!] you say to open '%s', but that isn't an RTL file, senpai! >///<", *argv);

	if (!(fd = fopen(*argv, "rb")))
		die("[!] failed to open '%s' for reading", *argv);
	
	/* enter directory of the provided file */
	dir_enter_file(*argv);

	poolsize = kb * 1024; // arbitrary size
	line = 1;

	// allocate zero-initialized memory
	Otext = text = calloc_safe(1, poolsize);
	Odata = data = calloc_safe(1, poolsize);
	Ostack = stack = calloc_safe(1, poolsize);
	Osymbols = symbols = calloc_safe(1, poolsize);

	old_text = text;

	src = "char else enum if int return sizeof while "
		  "fopen read fclose fprintf sprintf sscanf printf malloc free memset memcmp memcpy system strcat strcmp strcasecmp exit "
		  ZZXC_FUNC_NAME_STR
		  " void main "
	;

	// add keywords to symbol table
	i = Char;
	while (i <= While) {
		next();
		current_id[Token] = i++;
	}

	// add library to symbol table
	i = OPEN;
	while (i < INSTRUCTION_COUNT) {
		next();
		current_id[Class] = Sys;
		current_id[Type] = XC_INT;
		current_id[Value] = i++;
	}

	next(); current_id[Token] = Char; // handle void type
	next(); idmain = current_id; // keep track of main
	
	/* load source file */
	fseek(fd, 0, SEEK_END);
	src_sz = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	src = malloc_safe(src_sz+1);
	
	// read the source file
	if ((i = fread(src, 1, src_sz, fd)) != src_sz)
		die("[!] '%s' read error, senpai! >///<", *argv, i);
	src[i] = 0; // add EOF character
	fclose(fd);
	
	/* complain if it doesn't contain "int main(" in some form */
	{
		char *ss;
		char *Ssrc = src;
		
		while (1)
		{
			ss = strstr(Ssrc, "\t""main");
			if (!ss) ss = strstr(Ssrc, "\n""main");
			if (!ss) ss = strstr(Ssrc, "\r""main");
			if (!ss) ss = strstr(Ssrc, " ""main");
			if (!ss) goto L_fail_no_main;
			
			char *ssB = ss;
			char *ssMain = ss;
			
			/* ensure main is followed by non-space or '(' */
			ss += 1;
			while (*ss == ' ' || *ss == '\t' || *ss == '\n' || *ss == '\r')
				++ss;
			ss += 4;
			while (*ss == ' ' || *ss == '\t' || *ss == '\n' || *ss == '\r')
				++ss;
			if (*ss != '(')
				goto L_try_next;
			
			/* ensure main is preceded by int */
			if (ssB == Ssrc)
				goto L_try_next;
			while (ssB > Ssrc && (*ssB == ' ' || *ssB == '\t' || *ssB == '\n' || *ssB == '\r'))
				--ssB;
			if (ssB - 2 < Ssrc)
				goto L_try_next;
			if (memcmp(ssB-2, "int", 3))
				goto L_try_next;
			ssB -= 3;
			if (ssB == Ssrc || *ssB == ' ' || *ssB == '\t' || *ssB == '\n' || *ssB == '\r')
				break;
			
		L_try_next:
			/* try again, starting one position forward from last found */
			Ssrc = ss + 1;
			continue;
		L_fail_no_main:
			die("[!] '%s' is not a valid rtl file, senpai! >///< where's int main?", *argv);
		}
	}
	
	/* preprocessing */
	procIncludes(&src);
	src = preproc_oo_struct(src);
	
	src = preproc_remove_comments(src, 1);
	src = preproc_remove_token(src, "unsigned");
	src = preproc_remove_token(src, "const");
	src = preproc_overwrite_token(src, "FILE", "void");
	//src = preproc_overwrite_token(src, "(void)", "()");
	
	old_src = src;
	Osrc = src;

	program();

	if (!(pc = (int *)idmain[Value]))
		die("[!] undefined reference to main()");

#ifndef NDEBUG
	// dump_text();
	if (assembly) {
		// only for compile
		return 0;
	}
#endif
	
	// setup stack
	sp = (int *)((int)stack + poolsize);
	*--sp = EXIT; // call exit if main returns
	*--sp = PUSH; tmp = sp;
	*--sp = argc;
	*--sp = (int)argv;
	*--sp = (int)tmp;
	
	rval = eval();
	
	free(Osrc);
	free(Otext);
	free(Odata);
	free(Ostack);
	free(Osymbols);
	
	fprintf(stderr, "script executed successfully!\n");
	if (!no_getchar)
	{
#ifdef WIN32
		/* on windows, we wish to leave the window up afterwards */
		fprintf(stderr, "press enter key to exit\n");
		getchar();
#endif
	}

	return rval;
}

