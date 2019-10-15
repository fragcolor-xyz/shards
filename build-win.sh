#!/bin/sh

pacman -S --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-ninja mingw-w64-x86_64-clang
mkdir build
cd build
cmake -G Ninja -DBUILD_CORE_ONLY=1 -DCMAKE_BUILD_TYPE=Release ..
ninja cbl
./cbl ../src/tests/general.clj
./cbl ../src/tests/variables.clj
./cbl ../src/tests/subchains.clj
./cbl ../src/tests/linalg.clj