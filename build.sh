#!/bin/sh

set -e

git submodule update --init --recursive

./bootstrap

test -d build || mkdir build
test -d build/debug || mkdir build/debug
test -d build/release || mkdir build/release

pushd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -B./Debug ..
ninja -C Debug shards
popd build
