# build everything else
gcc -o zzrtl-debug src/*.c o/*.o -Wall -Wno-unused-variable -Wno-unused-function -lm -g -Og -lpthread


