#!/bin/bash
# Script to work around some issues when calling `cargo build`/`cargo rustc` from Xcode for iOS
#
# Based how corrosion sets the library path
# https://github.com/corrosion-rs/corrosion/blob/6a0d3ae99d8600eac7c693cb282cee4693cdc0b7/cmake/Corrosion.cmake#L873
# Without this, build scripts will not build, because it can not link them
#
# Additionally, bindgen needs to be passed the sysroot for the target platform to be able to find the system headers

set -ex
TARGET_SDKROOT=`xcrun --sdk $1 --show-sdk-path`
shift

export BINDGEN_EXTRA_CLANG_ARGS="-isysroot $TARGET_SDKROOT -isystem $TARGET_SDKROOT/usr/include/c++/v1 $BINDGEN_EXTRA_CLANG_ARGS"
export LIBRARY_PATH=`xcrun --sdk macosx --show-sdk-path`/usr/lib

$@
