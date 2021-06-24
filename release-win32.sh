# object files live here
mkdir -p o

# build compression functions (slow)
~/c/mxe/usr/bin/i686-w64-mingw32.static-gcc -c -DNDEBUG -s -Ofast -flto -lm -Wall src/enc/*.c src/enc/lzo/*.c src/enc/ucl/comp/*.c src/enc/apultra/*.c
mv *.o o

# build stb functions (slow)
~/c/mxe/usr/bin/i686-w64-mingw32.static-gcc -c -DNDEBUG src/stb/*.c -Wall -lm -s -Os -flto -lpthread -Wno-unused-function -Wno-unused-variable
mv *.o o

# build everything else
~/c/mxe/usr/bin/i686-w64-mingw32.static-gcc -o zzrtl.exe -DNDEBUG src/*.c o/*.o -Wall -lm -s -Os -flto -lpthread -Wno-unused-function -Wno-unused-variable

# move to bin directory
mkdir -p bin/win32
mv zzrtl.exe bin/win32
