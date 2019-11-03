#!/bin/sh

pacman -S --noconfirm base-devel mingw-w64-i686-toolchain mingw-w64-i686-cmake mingw-w64-i686-boost mingw-w64-i686-ninja mingw-w64-i686-clang wget

# bimg
cd deps/bimg/
export MINGW=/d/a/_temp/msys/msys32/mingw32 # chainblocks/setup-msys2, needed by bimg
make mingw-gcc-release32
cd ../../

# SDL
mkdir external
cd external
wget -O sdl.tar.gz https://www.libsdl.org/release/SDL2-devel-2.0.10-mingw.tar.gz
tar xzf sdl.tar.gz
cd ../

mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja cbl
./cbl ../src/tests/general.clj
./cbl ../src/tests/variables.clj
./cbl ../src/tests/subchains.clj
./cbl ../src/tests/linalg.clj
./cbl ../src/tests/loader.clj
