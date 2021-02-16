#!/bin/sh

# fail on errors
set -e

# rustup update # causes issues with CI
export RUSTUP_USE_CURL=1
rustup default stable-x86_64-pc-windows-gnu

pacman -S --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld wget

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
ninja rust_blocks && ninja cbl && ninja test_runtime

echo "Running test: general"
./cbl ../src/tests/general.edn
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
echo "Running test: pytest"
./cbl ../src/tests/pytest.clj
echo "Running test: genetic"
./cbl ../src/tests/genetic.clj
echo "Running test: http"
./cbl ../src/tests/http.clj
echo "Running test: brotli"
./cbl ../src/tests/brotli.clj
echo "Running test: snappy"
./cbl ../src/tests/snappy.clj
echo "Running test: ws"
./cbl ../src/tests/ws.clj
echo "Running test: bigint"
./cbl ../src/tests/bigint.clj
echo "Running test: bgfx"
./cbl ../src/tests/bgfx.clj
echo "Running test: wasm"
./cbl ../src/tests/wasm.clj
echo "Running test_runtime"
./test_runtime
