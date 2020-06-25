#!/bin/sh

# fail on errors
set -e

export CHAINBLOCKS_DIR=`pwd`

# snappy
# cd deps/snappy
# mkdir build
# cd build
# cmake -G Ninja ..
# ninja
# cd ../../../

# # SDL
# mkdir external
# cd external
# wget -O sdl.tar.gz https://www.libsdl.org/release/SDL2-devel-2.0.10-mingw.tar.gz
# tar xzf sdl.tar.gz
# cd ../

mkdir build
cd build
cmake -G Ninja -DBUILD_CORE_ONLY=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
ninja cbl && ninja cb_shared
./cbl ../src/tests/general.clj
./cbl ../src/tests/variables.clj
./cbl ../src/tests/subchains.clj
./cbl ../src/tests/linalg.clj
./cbl ../src/tests/loader.clj
./cbl ../src/tests/network.clj
./cbl ../src/tests/struct.clj
./cbl ../src/tests/flows.clj
# ./cbl ../src/tests/snappy.clj
./cbl ../src/tests/kdtree.clj

mkdir -p ../rust/target/debug
cp libcb_shared.so ../rust/target/debug/

cd ../rust

cargo test
# cargo test --features "blocks"
