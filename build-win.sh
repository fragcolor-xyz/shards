#!/bin/sh

# fail on errors
set -e

pacman -S --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld wget mingw-w64-x86_64-python

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
./cbl ../src/tests/kdtree.clj
./cbl ../src/tests/channels.clj
PYTHONHOME=/c/msys64/mingw64 ./cbl ../src/tests/pytest.clj
./cbl ../src/tests/lmdb.clj
./cbl ../src/tests/genetic.clj
./cbl ../src/tests/http.clj
