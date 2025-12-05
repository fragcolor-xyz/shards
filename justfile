set fallback

rust_toolchain := `cat rust.version`

# pull and update in one go
pull:
  git pull
  ./update.sh

build-docker-image:
  GIT_COMMIT=$(git rev-parse --short HEAD); docker buildx build \
   -f Dockerfile.shards \
   --build-arg GIT_COMMIT=$GIT_COMMIT \
   -t fragcolor/shards:latest \
   -t fragcolor/shards:$GIT_COMMIT \
   --push .

check-ci:
  claude -p "Claude, please check the CI for current branch PR, github actions. Somehow grep if there is any error and report pls"

# configure cmake in build/Debug
configure:
  cmake -GNinja -B build/Debug -DCMAKE_BUILD_TYPE=Debug > /dev/null

# configure cmake in build/Release
configure-rel:
  cmake -GNinja -B build/Release -DCMAKE_BUILD_TYPE=Release > /dev/null

# build shards (configures first if needed)
build: configure
  cmake --build build/Debug --target shards

# build shards (configures first if needed)
build-rel: configure-rel
  cmake --build build/Release --target shards

# build shards with filtered output (shows only errors and critical warnings)
build-quiet: configure
  #!/bin/bash
  set -o pipefail
  cmake --build build/Debug --target shards 2>&1 | \
    grep -v "^warning:" | \
    grep -v "^\[.*\].*\.o$" | \
    grep -v "^   Compiling" | \
    grep -v "^    Checking" | \
    grep -v "^     Finished" | \
    tee /tmp/shards-build.log || \
    (echo "Build failed. Full log: /tmp/shards-build.log" && tail -50 /tmp/shards-build.log && false)

# check just the rust union compilation
check-rust: configure
  #!/bin/bash
  export CARGO_TARGET_DIR=`pwd`/../tmp/rust-target
  export RUSTUP_TOOLCHAIN=`cat rust.version`
  export CRSQLITE_COMMIT_SHA=shards-dev
  cd build/Debug/src/union/shards-rust-union
  cargo check 2>&1 | grep -E "(error|localshell)" || echo "✓ Rust check passed"

[no-cd]
cargo-check:
  RUSTUP_TOOLCHAIN={{ rust_toolchain }} cargo check

tests:
  build/Debug/shards shards/tests/general.shs
  build/Debug/shards shards/tests/hello.shs

run-all-tests mode="Release":
  sh run-macos-gpu-tests.sh --with-cpu --shards-bin build/{{mode}}/shards

format:
  sh format.sh

check-union mode="Release" target="":
  #!/bin/bash
  echo "Checking target {{target}}"
  export CRSQLITE_COMMIT_SHA=shards-dev
  export RUSTUP_TOOLCHAIN=`cat rust.version`
  cd /Users/sugar/devel/shards/build/{{mode}}/src/union/shards-rust-union
  cargo check --all-features {{target}}

check-union-visionos: (check-union "Release" "--target aarch64-apple-visionos -Zbuild-std") # Does not work yet (some SSL issues)