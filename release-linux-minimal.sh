# build everything else
gcc -o zzrtl -DNDEBUG src/*.c o/*.o -Wall -Wno-unused-variable -Wno-unused-function -lm -s -Os -flto -lpthread

# move to bin directory
mkdir -p bin/linux64
mv zzrtl bin/linux64
