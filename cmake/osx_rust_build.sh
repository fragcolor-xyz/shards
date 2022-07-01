#!/bin/bash
set -ex
export SDKROOT=`xcrun --sdk macosx --show-sdk-path`
export BINDGEN_EXTRA_CLANG_ARGS="-isysroot $SDKROOT $BINDGEN_EXTRA_CLANG_ARGS"
$@