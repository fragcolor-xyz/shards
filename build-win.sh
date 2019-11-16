#!/bin/sh

pacman -S --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-ninja mingw-w64-x86_64-clang wget

# bimg
cd deps/bimg/
export MINGW=/d/a/_temp/msys/msys64/mingw64 # chainblocks/setup-msys2, needed by bimg
make mingw-gcc-release64
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
./cbl ../src/tests/network.clj
./cbl ../src/tests/struct.clj
./cbl ../src/tests/flows.clj
