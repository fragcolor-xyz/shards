#!/bin/bash

opt_short="d"
opt_long="debug"

OPTS=$(getopt -o "$opt_short" -l "$opt_long" -- "$@")

eval set -- "$OPTS"

# default values
build_type=Release

# get args
while true
do
    case "$1" in
        -r|--release)
            build_type=Release
            shift ;;
        -d|--debug)
            build_type=Debug
            shift ;;
        --) # End of input reading
            shift; break ;;
    esac
done

script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [ $build_type == "Debug" ];
then
    echo "Will run the samples using the debug version of shards"
    shards_exe="Debug/shards.exe"
else
    echo "Will run the samples using the release version of shards"
    shards_exe="Release/shards.exe"
    # required on Windows for release build
    . $script_dir/env.sh
fi

# execute commands
pushd $script_dir/docs/samples
for i in $(find shards -name '*.edn');
do
    echo "Running sample $i";
    $script_dir/build/$shards_exe run-sample.edn --file "$i" > >(tee "$i.log");
done
popd
