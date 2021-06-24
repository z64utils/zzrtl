### zzrtl expert guide

For those already familiar with the starter guide (below) who want bleeding-edge functionality, please check out [jaredemjohnson's fork](https://github.com/jaredemjohnson/zzrtl-audio). It adds support for sound, music, and text editing.

### zzrtl starter guide

This guide assumes you already have `zzrtl` (see [`releases`](https://github.com/z64me/zzrtl/releases)) and a clean OoT debug rom.

Note to Windows users: Use [Notepad++](https://notepad-plus-plus.org/) with C syntax highlighting for the optimal editing experience.

Now here are all the steps for dumping and building OoT:

Create a new folder.

Put your OoT debug rom in it. Name it `oot-debug.z64`. All lowercase, including the extension. If you don't like this name, edit `oot_dump.rtl`.

Save `oot_dump.rtl` and `oot_build.rtl` into the folder as well.

Linux users: use command line magic to execute `oot_dump.rtl`

Windows users:
The easiest thing you can do is drag-and-drop `oot_dump.rtl` onto `zzrtl.exe`. The second easiest thing you can do, which is more convenient long-term, would be to right-click `oot_dump.rtl` and associate all `.rtl` files with `zzrtl.exe`. Then you can execute any `.rtl` file by double-clicking it.

Once `oot_dump.rtl` finishes executing, `project.zzrpl` and some folders containing resources will be created. Now you can run `oot_build.rtl` to build a new rom.

The resulting rom will be `build.z64`, and its compressed counterpart will be `build-yaz.z64`. The initial compression will take a minute or two because it builds a cache, but subsequent compressions are faster. Other `zzrtl`-compatible codecs can be found [on the `z64enc` repo](https://github.com/z64me/z64enc). If you wish to disable compression, edit `oot_build.rtl`.

### rtl script specification
```
zzrtl uses a very minimalistic C implementation.
It is based on xc: https://github.com/lotabout/write-a-C-interpreter

The following C features are unsupported:
 - floating points          - for loops
 - the preprocessor         - goto
 - do-while                 - all variables must be
 - break                      declared at beginning
 - +=, -=, etc.               of function
 - unsigned types           - arrays
 - declare-anywhere         - assigning variables
 - switch-case                during declaration
 - the struct keyword is    - function prototypes
   reserved for built-in    - probably other stuff;
   types                      proceed with caution

Besides what you write yourself, the only functions available to you
are the ones listed below. A brief description is provided for each.
zzrtl's built-in error checking reports errors and ends execution if
anything goes wrong, though there are exceptions. If you find these
notes too cryptic, refer to the pre-made scripts. They are thoroughly
commented and show each function in action.


libc functions:
printf
sprintf
sscanf
malloc
free
fopen
fprintf
fclose
memset
memcmp
strcmp
strcasecmp
strcat
system
(most of these have had error checking added so the program will
 throw an error if an invalid pointer is used with them)


generic functions:
die(char *fmt, ...)     terminate with a custom error message
get32(void *)           get 32-bit (4 byte) value from raw data
get24(void *)           get 24-bit (3 byte) value from raw data
get16(void *)           get 16-bit (2 byte) value from raw data
get8(void *)            get 8-bit (1 byte) value from raw data
put32(void *, int)      put 32-bit (4 byte) value into raw data
put24(void *, int)      put 24-bit (3 byte) value into raw data
put16(void *, int)      put 16-bit (2 byte) value into raw data
put8(void *, int)       put 8-bit (1 byte) value into raw data
u8(char)                get unsigned equivalent of signed char
s8(int)                 get signed char equivalent of int
u16(int)                cast int to unsigned short
s16(int)                cast int to signed short
u32op(a, *op, b)        cast a and b to u32 and do operation;
                        example: u32op(apples, ">=", oranges)
                        the reason this exists is as a workaround for
                        situations where signedness may cause behavior
                        that is undesirable;
                        valid options for op:
                        "+"  "-"  "*"  "/" "&"  "|"  "<"  ">"  "<="
                        ">=" "==" "%"
substring(*list, idx)   get string at index `idx` in string `*list`;
                        returns 0 if anything goes awry
find_bytes              find ndl inside hay; ndl is a 0-term'd string
   (*hay, hay_sz        of hexadecimal characters (failure returns 0)
    , *ndl, only1)      NOTE: * can be used for wildcard bytes
                              example string "DEAD****BEEF"
                        only1 = 1 throws fatal error if more than one
                                occurrence is found
find_bytes_stride       same as find_bytes(), but allows you to specify
   (*hay, hay_sz, *ndl  stride; stride is the number of bytes to
    , stride, only1)    advance when searching
find_text               find ndl inside hay; ndl is a text string
   (*hay, hay_sz        (returns 0 on failure)
    , *ndl, only1)      example string "scubadiver"
                        only1 = 1 throws fatal error if more than one
                                occurrence is found
find_text_stride        same as find_text(), but allows you to specify
   (*hay, hay_sz, *ndl  stride; stride is the number of bytes to
   , stride, only1)     advance when searching
ovl_vram_sz             returns virtual ram size of overlay
   (void *, sz)
load_png                returns pointer to rgba8888 pixel data of png
   (fn, int *w, int *h) if it is successfully loaded; w and h are
                        propagated with its width and height as well
                        ex: pix = load_png("sky.png", &width, &height);
                        returns 0 if file doesn't exist
int_array(num, ...)     create an int array containing `num` elements
                        array32 = int_array(4, 10, 20, 30, 40);
                        array32[0] is now 10, [1] is 20, and so on
new_string(..., 0)      combines multiple strings and returns a pointer
                        to the result, which you can free() if you want
                        the list you provide must end with 0
                        ex:  new_string("build-", codec, ".z64", 0);
loadfile(               load a file, returning a pointer to its raw
   char *fn, int *sz    data, or 0 if it doesn't exist; if `optional`
    , bool optional)    is `true`, no error will be thrown if file does
                        not exist; `sz` will be set to size of file (in
                        bytes), or if you don't need that, pass 0
tsv_col_row(char *tsv   returns pointer to string inside tsv, beneath
   , char *col          column `col` and in row `row` (row 0 is first
   , int   row)         row where names are contained)


directory functions:
dir_exists(char *)      returns 1 if directory of given name exists,
                        returns 0 otherwise
dir_enter(char *)       enter a directory (folder)
                        NOTE: directory is created if it doesn't exist
dir_enter_file(char *)  enter the directory of provided file
dir_leave(void)         leave last-entered directory
dir_mkname              make a compliant directory name; str can be 0
   (int, *str)          if you want only a number for the folder name
dir_use_style(char *)   set style used by dir_mkname (default is "pre")
                        valid options:
                           "pre"   : uses form "%d - %s"
                           "preX"  : uses form "0x%X - %s"
                           "post"  : uses form "%s (%d)"
                           "postX" : uses form "%s (0x%X)"
file_exists(char *)     returns non-zero if file of given name exists,
                        returns 0 otherwise


struct rom functions:
.new(char *)            allocate a rom structure and load rom file
.free(void)             free a rom structure
.raw(ofs)               get pointer to raw data at offset `ofs` in rom
.save(char *)           save rom to disk using specified filename
.align(int)             align injected files such that their injection
                        offset is a multiple of the value you provide;
                        in retail roms, alignment is 0x1000 for every
                        file except the overlays, boot, dmadata, the
                        audio files, and link_animetion [sic] (it is
                        0x10 for all of these); the value you provide
                        must be a multiple of 16 (0x10); to maximize
                        space savings, inject files with smaller
                        alignment requirements first
.compress(fmt, mb)      compress rom using specified algorithm
                        valid options: "yaz", "lzo", "ucl", "aplib"
                        NOTE: to use another codec, patch your rom
                        with z64enc: https://github.com/z64me/z64enc
                        mb is the number of mb to cap the compressed
                        rom to; 32 is standard for OoT and MM
.seek(u32)              go to offset within rom
.seek_cur(adv)          go forward adv bytes in rom
                        NOTE: use a negative value to travel backwards
.tell(void)             get offset within rom
.size(void)             get size of rom
.inject_prefix()        the next file injected will have an empty prefix
                        (use dma_prefix() after inject() to get pointer
                        to prefix, which you can then write into)
.inject_mode(char*)     allows customizing file injection behavior;
                        valid options include:
                           "earliest" : use earliest available block
                                      (injects nearer to start of rom)
                           "latest"   : use latest available block
                                      (injects nearer to end of rom)
                           "smallest" : use smallest available block
                           "largest"  : use largest available block
                           "default"  : behaves the same as "smallest"
                           "0" or 0   : use "default"
.inject(fn, comp)       inject file into rom
                        NOTE: if comp is non-zero, file is compressible
                        NOTE: returns pointer to injected data, or 0
                        NOTE: if name is formatted like "*.ext", it
                              will auto-detect a file by extension; for
                              example, "*.zobj" to inject whatever zobj
                              it can find
.inject_dma             inject file into rom, over known dma index
   (fn, idx, comp)      (file-size must match file being overwritten)
.inject_png             loads PNG, converts to N64 format, and injects
   (fn, fmt, bpp, comp) into rom fmt and bpp use the n64texconv enums
                        NOTE: if comp is non-zero, file is compressible
.inject_raw             inject raw data into rom
   (*raw, sz, comp)     NOTE: if comp is non-zero, file is compressible
                        NOTE: returns pointer to injected data, or 0
                        NOTE: sz is number of bytes, raw points to them
.inject_raw_dma         inject raw data into rom, over known dma index
   (*raw, sz, idx, cmp) (sz must match entry being overwritten)
.file_start(void)       get start offset of data injected with inject()
                        NOTE: will be 0 if inject() failed
.file_end(void)         get end offset of data injected with inject()
                        NOTE: will be 0 if inject() failed
.file(void)             get pointer to data of most recently injected;
                        NOTE: will be 0 if inject() failed
.file_sz(void)          get size of injected data
                        NOTE: will be 0 if inject() failed
.file_dma(void)         get dma index of data injected with inject()
                        NOTE: will be -1 if inject failed
.extract                extract raw data to a file
   (fn, start, end)
.extract_png            converts raw texture data to rgba8888, saves
   (fn, buf*, tex, pal  as PNG; buf can be a prealloc'd block that you
    w, h, fmt, bpp)     can guarantee is large enough to intermediately
                        store the converted texture, or 0 to tell the
                        function to alloc its own; fmt and bpp must be
                        as they are defined by the n64texconv enums
.dma(start, num)        specify start of dmadata, and number of entries
                      * every entry is by default marked as readonly,
                        and dma_queue() must be used to mark specific
                        entries as writable
                      * no entry is queued for compression at first,
                        and dma_compress() must be used to selectively
                        enable compression where it is desired
.dma_queue_one(idx)     mark one dma entry (by index) as writable;
.dma_queue              mark dma indices as writable, between start and
   (start, end)         end, inclusive (aka start <= idx <= end)
.dma_compress_one       set compression flag on dma entry (by index);
   (idx, comp)          if (comp == 1), file is marked for compression;
                        0 means no compression; other non-zero values
                        are reserved for internal use only
.dma_compress           set compression flag on indices between start
   (start, end, comp)   and end, inclusive (aka start <= idx <= end)
.dma_prefix()           returns pointer to prefix of last injected file
.dma_register(offset)   register rom offset as a location to write the
                        dma index of the most recently injected file
                        (you can register multiple offsets)
.dma_offset(n)          returns offset of file described by nth dmaentry
.dma_ready(void)        call this when you're finished with the dma
                        stuff; must be called before you inject data
.write(void *, sz)      write and advance `sz` bytes within rom
.write32(u32)           write and advance four  bytes within rom
.write24(int)           write and advance three bytes within rom
.write16(int)           write and advance two   bytes within rom
.write8(int)            write and advance one   byte  within rom
.read32(void)           read and advance four  bytes within rom
.read24(void)           read and advance three bytes within rom
.read16(void)           read and advance two   bytes within rom
.read8(void)            read and advance one   byte  within rom
.cloudpatch(ofs, fn)    patch a rom with a cloudpatch (.txt patch);
                        ofs is the value to add to all the offsets in
                        the patch, which is useful for applying patches
                        to individual files as they are injected;
                        otherwise, just use 0
.rearchive              this function is for MM only; it re-encodes
   (start, end, old,    the contents of an archive (a file containing
    new, repack)        multiple compressed files); `old` refers to
                        the old encoding (only "yaz" is supported for
                        now), and `new` refers to the new encoding,
                        like "ucl"; `repack` should always be false (0)
                        unless you modify MM's packing functionality;
                        this makes it so the re-encoded files fit at
                        the offsets of the original files within the
                        archives (this is to get around MM's hard-coded
                        references to the assets; if you happen to use
                        custom code for MM archive parsing, you can use
                        true (1)
.rearchive_one          re-encode one archive, by dma index
   (idx, old, new,      (MM only; see rearchive for more details)
    repack)


struct folder functions:
.new(*ext)              allocate a folder structure and parse the
                        current directory; contents can be named any
                      . way you like, as long as they contain a number
                         > "0 - gameplay_keep"
                         > "gameplay_keep (0)"
                         > "room_0.zmap"
                         > etc.
                      . folder/file names starting with a '.' or '_'
                        are not processed
                         > .trash
                         > _src
                         > etc.
                      . items are accessed in the order specified by
                        the numerical part, which can be hexadecimal as
                        long as it is preceded by "0x"
                      . no two folder/file names are allowed to contain
                        the same number (a fatal error will be thrown)
                      . if ext is 0, it creates a folder list
                      . if ext is non-zero, it creates a list of files
                        of the requested extension, such as "zmap"
                      . it first tries to find a number at the very
                        beginning (it is not allowed to be surrounded
                        by quotes, parenthesis, preceded by a space,
                        etc); if that fails, it uses the last number it
                        detects in the name
                         -> "1 - spot04"        yields index 1
                         -> "En_Torch2 (51)"    yields index 51
                         -> "object_link_boy"   throws fatal error
.free(void)             free a folder structure
.name(void)             get current folder name
.index(void)            get numerical part of current folder name
.next(void)             go to next folder in list;
                        returns 0 when end of list is reached
.count(void)            get number of items in list
.remaining(void)        get number of items between current and end
.max(void)              get highest index detected in list


struct conf functions:
.new(*fn, *fmt)         allocate a conf structure and parse file;
                        fmt should be either "table" or "list";
                      * a fatal error occurs if the file doesn't exist
                      * see conf section for more details
.free(void)             free a conf structure
.exists(*name)          returns non-zero if name exists
.get_int(*name)         get integer value associated with name;
                      * this function throws a fatal error if the name
                        does not exist; if you are handling optional
                        names, use .exists(name) to confirm it exists
                        before calling this function
                      * passing 0 to this function will cause it to
                        return the current list item's value as an int
.get_str(*name)         get pointer to string associated with name;
                      * the contents of the string are read only; do
                        not modify them or you will cause undefined
                        behavior
                      * this function throws a fatal error if the name
                        does not exist; if you are handling optional
                        names, use .exists(name) to confirm it exists
                        before calling this function
.next(void)             in the case of a table, go to next row;
                        in a list, go to next item
                        returns 0 when there is no next line
.remaining(void)        returns non-zero if there are rows (in table),
                        items (in list), remaining, 0 otherwise
.name(void)             returns string of selected item name
                        returns 0 at end of list
                        (for use in list types only)
.value(void)            returns string of selected item value
                        returns 0 at end of list
                        (for use in list types only)


conf section

conf can load files of two formats: "table" and "list"

the following information applies to both formats:

you can use // to comment out the remainder of a line,
or /* to comment out a specific block of text */
the number of tabs or spaces between names/values does
not matter, as all contiguous blocks of whitespace are
used to determine where one name or value ends and the
next begins; the names and values can contain anything
but whitespace (you can get around this limitation by
putting quotes around them); lastly, keep in mind that
the names are NOT cAsE-sEnSiTiVe, and as such, "vram"
and "VRAM" are the same

the source code provided for each can be executed using
zzrtl; just save as .rtl and open in zzrtl; don't forget
to make sure their dependencies ("table.txt", "list.txt")
are in the same directory as their respective .rtls

table notes

here is a table that you may load from a file

table.txt
scene   card   music   notes              fadein
0x02     off    stop   "diligent work"         2
0x03      on      go   "getting ocarina"       3

the first row contains the names that are used to look
up the values using the .get_x() functions; the rows
that follow contain the values themselves

the code for parsing it would look like

/****************************************
 * <z64.me> zzrtl table parsing example *
 ****************************************/

/* table.txt (save in same directory as example)
scene   card   music   notes              fadein
0x02     off    stop   "diligent work"         2
0x03      on      go   "getting ocarina"       3
*/

int
main(int argc, char **argv)
{
    struct conf *table;
    char *card;
    char *music;
    char *notes;
    int   scene;
    int   fadein;

    /* load the table */
    table = table.new("table.txt", "table");

    /* now parse all the rows */
    while (table.remaining())
    {
        /* retrieve variables */
        scene  = table.get_int("scene");
        card   = table.get_str("card");
        music  = table.get_str("music");
        fadein = table.get_int("fadein");
        notes  = table.get_str("notes");
       
        /* do something with the variables here */
        printf("scene %d settings:\n", scene);
        printf(" > card: %s\n", card);
        printf(" > music: %s\n", music);
        printf(" > fadein: %d\n", fadein);
        printf(" > notes: %s\n", notes);
       
        /* on the first pass, each variable will be whatever the  *
         * first value row says it should be; on the second pass, *
         * it pulls from the next row; there is no third pass     */
       
        /* go to next row */
        table.next();
    }

    /* cleanup */
    table.free();
    return 0;
}


list notes

here is a list that you may load from a file

list.txt
vram          0x80800000
unknown       0x01000000
"please do"   "work diligently"
//optional      "output changes if you uncomment this line"

each row contains a names is used to look up the value
to its right using the .get_x() functions;

the code for parsing it would look like

/***************************************
 * <z64.me> zzrtl list parsing example *
 ***************************************/

/* list.txt (save in same directory as example)
vram          0x80800000
unknown       0x01000000
"please do"   "work diligently"
//optional      "output changes if you uncomment this line"
*/

int
main(int argc, char **argv)
{
    struct conf *list;
    char *optional;
    char *pleasedo;
    int   vram;
    int   unknown;

    /* load the list */
    list = list.new("list.txt", "list");

    /* retrieve variables */
    vram      = list.get_int("vram");
    unknown   = list.get_int("unknown");
    pleasedo  = list.get_str("please do");
   
    /* how to do optional variables */
    if (list.exists("optional"))
        optional = list.get_str("optional");
    else
        optional = 0;

    /* do something with the variables here */
    printf("vram:          0x%X\n", vram);
    printf("unknown:       0x%08X\n", unknown);
    printf("'please do':  '%s'\n", pleasedo);
    if (optional)
        printf("optional:     '%s'\n", optional);

    /* cleanup */
    list.free();
    return 0;
}


migrating a zzromtool project tree to zzrtl

first off, if you are taking advantage of zzromtool's
built-in table expansion, you are going to need to either:
 (a) make everything fit within the original
     limits for each table
 (b) create your own table expansion mod and
     update the sample build script as needed

misc, system, shader, patch, repoint.tsv
 > this stuff is all deprecated, in favor of modifying
   it in the rom directly; should you wish to reimplement
   it in your own build script, the option is there
 > externalizing some files, like Link's overlay, does
   have its benefits; it was left out of the example
   build script for the sake of simplicity, but here is
   a guide to adding this particular functionality to
   the sample build script:
   https://github.com/z64me/zzrtl/compare/master...oot-link-cat


route.txt
 > remove the '#' character from the first row
 > any remaining '#' comments should be changed to //

scene
in the sample build script...
 > change "unk_a" to "unk-a:"
 > change "unk_b" to "unk-b:"
 > change "shader" to "shader:"
note:
 > save and restrict are not used, but you could add
   support for them with some assembly editing and by
   updating the build script

If you read this far, chances are you don't even need a manual. ;)
```

