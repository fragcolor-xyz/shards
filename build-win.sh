#!/bin/sh

pacman -S --noconfirm ninja cmake mingw-w64-x86_64-boost
mkdir build
cd build
cmake -G Ninja -DBUILD_CORE_ONLY=1 -DCMAKE_BUILD_TYPE=Debug ..
ninja cbl