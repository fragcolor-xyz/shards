#!/bin/bash
set -ex
SDKROOT=`xcrun --sdk macosx --show-sdk-path` $@