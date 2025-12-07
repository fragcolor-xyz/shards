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
  sh run-tests.sh --with-cpu --shards-bin build/{{mode}}/shards

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

# ==================== Emscripten Build ====================
# Path to emsdk - override with: just emsdk_path=/path/to/emsdk configure-wasm
emsdk_path := env_var_or_default("EMSDK_PATH", "../emsdk")
# emsdk 4.0.10+ required for --use-port=emdawnwebgpu (modern WebGPU API matching wgpu v27)
emsdk_version := "4.0.10"

# configure cmake for emscripten/wasm build
configure-wasm:
  #!/bin/bash
  set -e

  # Check emsdk exists
  if [ ! -d "{{ emsdk_path }}" ]; then
    echo "Error: emsdk not found at {{ emsdk_path }}"
    echo "Clone it with: git clone https://github.com/emscripten-core/emsdk.git {{ emsdk_path }}"
    exit 1
  fi

  # Setup emsdk (4.0.10+ required for emdawnwebgpu port)
  pushd "{{ emsdk_path }}"
  ./emsdk install {{ emsdk_version }}
  ./emsdk activate {{ emsdk_version }}
  source ./emsdk_env.sh
  export EM_CONFIG=$PWD/.emscripten
  export EMSCRIPTEN_ROOT=$PWD/upstream/emscripten
  popd

  # Setup rust target
  export RUSTUP_TOOLCHAIN=`cat rust.version`
  rustup +$RUSTUP_TOOLCHAIN target add wasm32-unknown-emscripten
  rustup +$RUSTUP_TOOLCHAIN component add rust-src

  # Setup host toolchain for cross-compilation
  export HOST_CC=$(which cc)
  export HOST_AR=$(which ar)

  cmake -Bbuild/Wasm -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSKIP_HEAVY_INLINE=1 \
    -DUSE_LTO=0 \
    -DRUST_USE_LTO=0 \
    -DEMSCRIPTEN_PTHREADS=ON \
    -DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN_ROOT/cmake/Modules/Platform/Emscripten.cmake

# build shards for wasm (configures first if needed)
build-wasm: configure-wasm
  cmake --build build/Wasm --target shards

# quick build wasm (skips configure if already done)
build-wasm-quick:
  cmake --build build/Wasm --target shards

# setup wasm test dependencies (npm install, puppeteer)
setup-wasm-tests:
  #!/bin/bash
  set -e
  pushd shards/tests/web
  npm install
  npx puppeteer browsers install chrome@latest
  popd
  echo "Wasm test dependencies installed"

# run wasm tests locally (requires: build, build-wasm, setup-wasm-tests)
# Usage: just test-wasm                    # run all tests
#        just test-wasm gfx-cube.shs       # run single test
test-wasm *tests:
  #!/bin/bash
  set -e

  # Check for host shards binary
  if [ -f "build/Debug/shards" ]; then
    export shards=$(pwd)/build/Debug/shards
  elif [ -f "build/Release/shards" ]; then
    export shards=$(pwd)/build/Release/shards
  else
    echo "Error: No host shards binary found. Run 'just build' first."
    exit 1
  fi

  # Check for wasm build
  if [ ! -f "build/Wasm/shards-mt.js" ]; then
    echo "Error: No wasm build found. Run 'just build-wasm' first."
    exit 1
  fi
  export SHARDS_BUILD=$(pwd)/build/Wasm

  # Check for node_modules
  if [ ! -d "shards/tests/web/node_modules" ]; then
    echo "Error: Node modules not installed. Run 'just setup-wasm-tests' first."
    exit 1
  fi

  pushd shards/tests/web

  source ./shared

  function queue_test() {
    control action:run data:shards/tests/$1
  }

  # Spawn the test server in background
  ./run_server &
  SERVER_PID=$!
  trap "kill $SERVER_PID 2>/dev/null" EXIT

  sleep 2

  # Queue tests in background
  (
    if [ -n "{{ tests }}" ]; then
      # Run specific tests
      for test in {{ tests }}; do
        queue_test $test
      done
    else
      # Run all standard tests
      queue_test gfx-cube.shs
      queue_test gfx-texture.shs
      queue_test gfx-gltf.shs
      queue_test gfx-gltf-pack.shs
      queue_test gfx-gltf-anim.shs
      queue_test gfx-shader-translator-0.shs
      queue_test gfx-shader-translator-1.shs
      queue_test gfx-shader-translator-2.shs
      queue_test gfx-shader-translator-3.shs
      queue_test gfx-shader-translator-4.shs
      queue_test gfx-queue.shs
      queue_test gfx-read-texture.shs
      queue_test gfx-pbr.shs
      queue_test ui-0.shs
      queue_test ui-1.shs
      queue_test ui-2.shs
      queue_test general.shs@/tmp
      queue_test zip-map.shs
      queue_test strings.shs
      queue_test table-compose.shs
      queue_test variables.shs
      queue_test subwires.shs@/tmp
      queue_test linalg.shs
      queue_test math.shs
      queue_test math_audio.shs
      queue_test network-ws.shs
      queue_test struct.shs
      queue_test flows.shs
      queue_test channels.shs
      queue_test imaging.shs
      queue_test http.shs@/tmp
      queue_test bigint.shs
      queue_test brotli.shs
      queue_test snappy.shs
      queue_test expect.shs
      queue_test rust.shs
      queue_test crypto.shs
      queue_test wire-macro.shs
      queue_test branch.shs
      queue_test audio2.shs
      queue_test events.shs
      queue_test complex-deserialize.shs
      queue_test db.shs@/tmp
      queue_test suspend-resume.shs
      queue_test whendone.shs
      queue_test return.shs
      queue_test table-seq-push.shs
      queue_test failures.shs@/tmp
      queue_test traits.shs
    fi
    control action:shutdown
  ) &

  sleep 3

  # Run the browser
  node ./run_browser.js