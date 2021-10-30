#!/bin/sh

# fail on errors
set -e

# rustup update # causes issues with CI
export RUSTUP_USE_CURL=1
rustup default stable-i686-pc-windows-gnu

pacman -S --needed --noconfirm base-devel mingw-w64-i686-toolchain mingw-w64-i686-cmake mingw-w64-i686-boost mingw-w64-i686-ninja mingw-w64-i686-clang mingw-w64-i686-lld wget

mkdir build
cd build
# bindgen won't work on CI properly when building for win 32 bit
# run bindgen on a 64 bit tool chain with target for i686 first
cmake -G Ninja -DCMAKE_BUILD_TYPE=$1 -DSKIP_RUST_BINDGEN=1 ..
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
# echo "Running test: genetic"
# ./cbl ../src/tests/genetic.clj
echo "Running test: http"
./cbl ../src/tests/http.clj
echo "Running test: brotli"
./cbl ../src/tests/brotli.clj
echo "Running test: snappy"
./cbl ../src/tests/snappy.clj
# echo "Running test: ws"
# ./cbl ../src/tests/ws.clj
echo "Running test: bigint"
./cbl ../src/tests/bigint.clj
echo "Running test: bgfx"
./cbl ../src/tests/bgfx.clj
echo "Running test: wasm"
./cbl ../src/tests/wasm.clj
echo "Running test: eth"
./cbl ../src/tests/eth.edn
echo "Running test: crypto"
./cbl ../src/tests/crypto.edn
echo "Running test_runtime"
./test_runtime