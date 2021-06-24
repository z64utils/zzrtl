# object files live here
mkdir -p o

# build compression functions (slow)
gcc -c -DNDEBUG -s -Ofast -flto -lm -Wall src/enc/*.c src/enc/lzo/*.c src/enc/ucl/comp/*.c src/enc/apultra/*.c
mv *.o o

# build stb functions (slow)
gcc -c -DNDEBUG src/stb/*.c -Wall -lm -s -Os -flto -lpthread -Wno-unused-function -Wno-unused-variable
mv *.o o

# build everything else
gcc -o zzrtl -DNDEBUG src/*.c o/*.o -Wall -lm -s -Os -flto -lpthread -Wno-unused-function -Wno-unused-variable

# move to bin directory
mkdir -p bin/linux64
mv zzrtl bin/linux64
