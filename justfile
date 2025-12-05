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

# configure cmake with code coverage enabled
configure-cov:
  cmake -GNinja -B build/Coverage -DCMAKE_BUILD_TYPE=Debug -DCODE_COVERAGE=1 > /dev/null

# build shards with code coverage (configures first if needed)
build-cov: configure-cov
  cmake --build build/Coverage --target shards

# generate coverage report after running tests
coverage output_dir="coverage":
  #!/bin/bash
  set -e
  mkdir -p {{output_dir}}
  echo "Capturing coverage data..."
  lcov \
    --capture \
    --directory build/Coverage/src \
    --output-file {{output_dir}}/coverage.info \
    --ignore-errors inconsistent,gcov,range,format,count,category
  echo "Filtering out external dependencies..."
  lcov \
    --remove {{output_dir}}/coverage.info "*/c++/*" "*/boost/*" "*/usr/*" "*/deps/*" \
    --output-file {{output_dir}}/coverage-filtered.info \
    --ignore-errors inconsistent,gcov,range,format,count,category
  echo "Converting to relative paths..."
  sed -i '' -e "s#${PWD}/#./#g" {{output_dir}}/coverage-filtered.info
  echo "Generating HTML report..."
  genhtml {{output_dir}}/coverage-filtered.info \
    --output-directory {{output_dir}}/html \
    --ignore-errors inconsistent,gcov,range,format,count,category
  echo "Coverage report generated at {{output_dir}}/html/index.html"

# reset coverage counters (run before tests for clean measurement)
coverage-reset:
  #!/bin/bash
  echo "Resetting coverage counters..."
  lcov --zerocounters --directory build/Coverage/src --ignore-errors inconsistent,gcov 2>/dev/null || true
  find build/Coverage -name "*.gcda" -delete 2>/dev/null || true
  echo "Coverage counters reset"

# generate Rust coverage report after running tests
coverage-rust output_dir="coverage":
  #!/bin/bash
  set -e
  mkdir -p {{output_dir}}
  echo "Merging Rust profraw files..."
  find build/Coverage -name "*.profraw" -print0 | xargs -0 xcrun llvm-profdata merge -sparse -o {{output_dir}}/rust.profdata
  echo "Generating Rust coverage report..."
  # Find the rust static library to use as the binary for coverage
  RUST_LIB=$(find build/Coverage -name "libshards_rust_union.a" | head -1)
  if [ -z "$RUST_LIB" ]; then
    echo "Error: Could not find libshards_rust_union.a"
    exit 1
  fi
  xcrun llvm-cov show \
    --instr-profile={{output_dir}}/rust.profdata \
    --object "$RUST_LIB" \
    --format=html \
    --output-dir={{output_dir}}/rust-html \
    --ignore-filename-regex="/.cargo/|/rustc/|/deps/"
  echo "Rust coverage report generated at {{output_dir}}/rust-html/index.html"

# reset Rust coverage data
coverage-rust-reset:
  #!/bin/bash
  echo "Resetting Rust coverage data..."
  find build/Coverage -name "*.profraw" -delete 2>/dev/null || true
  echo "Rust coverage data reset"