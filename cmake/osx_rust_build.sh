#!/bin/bash
set -ex
TARGET_SDKROOT=`xcrun --sdk $1 --show-sdk-path`
shift
export SDKROOT=`xcrun --sdk macosx --show-sdk-path`
export BINDGEN_EXTRA_CLANG_ARGS="-isysroot $TARGET_SDKROOT -isystem $TARGET_SDKROOT/usr/include/c++/v1 $BINDGEN_EXTRA_CLANG_ARGS"
$@
