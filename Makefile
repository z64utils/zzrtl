 

# Disables built-in rules (e.g. how to make .o from .c)
.SUFFIXES:

.PHONY: default clean debug release

default: release

COMMON_FLAGS := -Wall -Wno-unused-variable -Wno-unused-function
LINK_FLAGS := -lm -lpthread
DEBUG_FLAGS := -g -Og
RELEASE_FLAGS := -DNDEBUG=1 -s -flto
RELEASE_ENC_FLAGS := -Ofast
RELEASE_OTHER_FLAGS := -Os

ENC_SRC := $(wildcard src/enc/*.c) $(wildcard src/enc/lzo/*.c) $(wildcard src/enc/ucl/comp/*.c) $(wildcard src/enc/apultra/*.c)
STB_SRC := $(wildcard src/stb/*.c) 
MAIN_SRC := $(wildcard src/*.c)

ENC_HDR := $(wildcard src/enc/*.h) $(wildcard src/enc/lzo/*.h) $(wildcard src/enc/ucl/comp/*.h) $(wildcard src/enc/apultra/*.h)
STB_HDR := $(wildcard src/stb/*.h) 
MAIN_HDR := $(wildcard src/*.h)

ENC_OBJ := $(addprefix build/,$(ENC_SRC:%.c=%.o))
STB_OBJ := $(addprefix build/,$(STB_SRC:%.c=%.o))
MAIN_OBJ := $(addprefix build/,$(MAIN_SRC:%.c=%.o))

PLATFORM := $(shell uname)

ifeq ($(PLATFORM),Linux)
	CC := gcc
	EXECUTABLE := zzrtl
else
	CC := i686-w64-mingw32.static-gcc
	EXECUTABLE := zzrtl.exe
endif

debug: CCFLAGS_ENC := $(COMMON_FLAGS) $(DEBUG_FLAGS)
debug: CCFLAGS_OTHER := $(COMMON_FLAGS) $(DEBUG_FLAGS)
debug: $(EXECUTABLE)
release: CCFLAGS_ENC := $(COMMON_FLAGS) $(RELEASE_FLAGS) $(RELEASE_ENC_FLAGS)
release: CCFLAGS_OTHER := $(COMMON_FLAGS) $(RELEASE_FLAGS) $(RELEASE_OTHER_FLAGS)
release: $(EXECUTABLE)

build/src/enc/%.o: src/enc/%.c $(ENC_HDR)
	@mkdir -p build/src/enc
	@mkdir -p build/src/enc/lzo
	@mkdir -p build/src/enc/ucl/comp
	@mkdir -p build/src/enc/apultra
	$(CC) $(CCFLAGS_ENC) -c $< -o $@

build/src/stb/%.o: src/stb/%.c $(STB_HDR)
	mkdir -p build/src/stb
	$(CC) $(CCFLAGS_OTHER) -c $< -o $@

build/src/%.o: src/%.c $(MAIN_HDR)
	mkdir -p build/src
	$(CC) $(CCFLAGS_OTHER) -c $< -o $@

$(EXECUTABLE): $(ENC_OBJ) $(STB_OBJ) $(MAIN_OBJ)
	$(CC) -o $@ $^ $(LINK_FLAGS)

clean:
	rm -rf build/ o/ bin/ $(EXECUTABLE)
