#!/bin/sh

# fail on errors
set -e

pacman -S --needed --noconfirm base-devel mingw-w64-i686-toolchain mingw-w64-i686-cmake mingw-w64-i686-boost mingw-w64-i686-ninja mingw-w64-i686-clang mingw-w64-i686-lld wget mingw-w64-i686-python

# bimg
cd deps/bimg/
export MINGW=/d/a/_temp/msys/msys64/mingw32 # chainblocks/setup-msys2, needed by bimg
# hack to cope with terrible bimg build system
ln -s $MINGW/bin/i686-w64-mingw32-g++.exe $MINGW/bin/x86_64-w64-mingw32-g++.exe
ln -s $MINGW/bin/i686-w64-mingw32-gcc.exe $MINGW/bin/x86_64-w64-mingw32-gcc.exe
make mingw-gcc-release32 > /dev/null 2>&1
cd ../../

# snappy
cd deps/snappy
mkdir build32
cd build32
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
# PYTHONHOME=/d/a/_temp/msys/msys64/mingw32 ./cbl ../src/tests/pytest.clj
./cbl ../src/tests/lmdb.clj
./cbl ../src/tests/genetic.clj
