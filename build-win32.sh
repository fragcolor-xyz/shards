#!/bin/sh

# fail on errors
set -e

pacman -S --needed --noconfirm base-devel mingw-w64-i686-toolchain mingw-w64-i686-cmake mingw-w64-i686-boost mingw-w64-i686-ninja mingw-w64-i686-clang mingw-w64-i686-lld wget mingw-w64-i686-python

# setup libbacktrace
cd deps/libbacktrace
mkdir build
./configure --prefix=`pwd`/build
make
make install
cd -

mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=$1 -DUSE_LIBBACKTRACE=1 ..
ninja cbl

echo "Running test: general"
./cbl ../src/tests/general.clj
echo "Running test: variables"
./cbl ../src/tests/variables.clj
echo "Running test: subchains"
./cbl ../src/tests/subchains.clj
echo "Running test: linalg"
./cbl ../src/tests/linalg.clj
echo "Running test: loader"
./cbl ../src/tests/loader.clj
echo "Running test: network"
./cbl ../src/tests/network.clj
echo "Running test: struct"
./cbl ../src/tests/struct.clj
echo "Running test: flows"
./cbl ../src/tests/flows.clj
echo "Running test: kdtree"
./cbl ../src/tests/kdtree.clj
echo "Running test: channels"
./cbl ../src/tests/channels.clj
# echo "Running test: pytest"
# PYTHONHOME=/c/msys64/mingw64 ./cbl ../src/tests/pytest.clj
echo "Running test: genetic"
./cbl ../src/tests/genetic.clj
echo "Running test: http"
./cbl ../src/tests/http.clj
echo "Running test: brotli"
./cbl ../src/tests/brotli.clj
echo "Running test: ws"
./cbl ../src/tests/ws.clj
echo "Running test: bigint"
./cbl ../src/tests/bigint.clj
echo "Running test: bgfx"
./cbl ../src/tests/bgfx.clj