#!/bin/sh

set -e

git submodule update --init --recursive

./bootstrap

mkdir -p build/debug

cmake -Bbuild/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug --target shards
