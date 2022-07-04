#!/bin/bash
# Script to work around some issues when calling `cargo build`/`cargo rustc` from Xcode for iOS
#
# The SDKROOT needs to be set to the HOST SDKROOT so that build scripts are able to build
#  if not, it results in linker errors similar to this issue https://github.com/TimNN/cargo-lipo/issues/41
#
# Additionally, bindgen needs to be passed the sysroot for the target platform to be able to find the system headers

set -ex
TARGET_SDKROOT=`xcrun --sdk $1 --show-sdk-path`
shift
export SDKROOT=`xcrun --sdk macosx --show-sdk-path`
export BINDGEN_EXTRA_CLANG_ARGS="-isysroot $TARGET_SDKROOT -isystem $TARGET_SDKROOT/usr/include/c++/v1 $BINDGEN_EXTRA_CLANG_ARGS"
$@
