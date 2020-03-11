#!/bin/sh

# fail on errors
set -e

pacman -S --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld wget mingw-w64-x86_64-python

# bimg
cd deps/bimg/
export MINGW=/d/a/_temp/msys/msys64/mingw64 # chainblocks/setup-msys2, needed by bimg
make mingw-gcc-release64 > /dev/null 2>&1
cd ../../

# snappy
cd deps/snappy
mkdir build
cd build
cmake -G Ninja ..
ninja
cd ../../../

# SDL
mkdir external
cd external
wget -O sdl.tar.gz https://www.libsdl.org/release/SDL2-devel-2.0.10-mingw.tar.gz
tar xzf sdl.tar.gz
cd ../

mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=$1 ..
ninja cbl
./cbl ../src/tests/general.clj
./cbl ../src/tests/variables.clj
./cbl ../src/tests/subchains.clj
./cbl ../src/tests/linalg.clj
./cbl ../src/tests/loader.clj
./cbl ../src/tests/network.clj
./cbl ../src/tests/struct.clj
./cbl ../src/tests/flows.clj
./cbl ../src/tests/snappy.clj
./cbl ../src/tests/stack.clj
./cbl ../src/tests/kdtree.clj
./cbl ../src/tests/channels.clj
./cbl ../src/tests/pytest.clj
