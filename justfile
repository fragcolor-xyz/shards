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
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
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

# ==================== JSPI Build (experimental) ====================
# Uses JSPI instead of Asyncify for fiber support - requires Chrome 137+/Firefox 139+

# configure cmake for JSPI wasm build
configure-wasm-jspi:
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

  # NOTE: JSPI + pthreads have compatibility issues during static initialization.
  # This build may fail at runtime with "trying to suspend without WebAssembly.promising"
  # Keeping for experimental/future use when Emscripten fixes JSPI+pthreads.
  # See: https://github.com/emscripten-core/emscripten/issues/19287
  cmake -Bbuild/WasmJspi -GNinja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DSKIP_HEAVY_INLINE=1 \
    -DUSE_LTO=0 \
    -DRUST_USE_LTO=0 \
    -DEMSCRIPTEN_PTHREADS=ON \
    -DSHARDS_USE_JSPI=ON \
    -DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN_ROOT/cmake/Modules/Platform/Emscripten.cmake

# build shards for wasm with JSPI (configures first if needed)
build-wasm-jspi: configure-wasm-jspi
  cmake --build build/WasmJspi --target shards

# quick build wasm JSPI (skips configure if already done)
build-wasm-jspi-quick:
  cmake --build build/WasmJspi --target shards

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

  # Check for wasm build (use SHARDS_BUILD if set, otherwise default to build/Wasm)
  if [ -z "$SHARDS_BUILD" ]; then
    export SHARDS_BUILD=$(pwd)/build/Wasm
  fi
  if [ ! -f "$SHARDS_BUILD/shards-mt.js" ]; then
    echo "Error: No wasm build found at $SHARDS_BUILD. Run 'just build-wasm' first."
    exit 1
  fi

  # Check for node_modules
  if [ ! -d "shards/tests/web/node_modules" ]; then
    echo "Error: Node modules not installed. Run 'just setup-wasm-tests' first."
    exit 1
  fi

  pushd shards/tests/web

  source ./shared

  function queue_test() {
    echo ">>> Queuing test: $1"
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

# run wasm tests in debug mode (keeps browser open on error, DevTools auto-open)
# Usage: just test-wasm-debug                    # run all tests
#        just test-wasm-debug gfx-cube.shs       # run single test
test-wasm-debug *tests:
  DEBUG_WASM=1 just test-wasm {{ tests }}

# run wasm tests with JSPI build (requires Chrome 137+/Firefox 139+)
# Usage: just test-wasm-jspi                    # run all tests
#        just test-wasm-jspi gfx-cube.shs       # run single test
test-wasm-jspi *tests:
  #!/bin/bash
  # Find newest Chrome version in puppeteer cache (JSPI needs 137+)
  CHROME_PATH=$(ls -d ~/.cache/puppeteer/chrome/mac_arm-*/chrome-mac-arm64/*.app/Contents/MacOS/* 2>/dev/null | sort -V | tail -1)
  if [ -n "$CHROME_PATH" ]; then
    echo "Using Chrome: $CHROME_PATH"
    export PUPPETEER_EXECUTABLE_PATH="$CHROME_PATH"
  fi
  export SHARDS_BUILD=$(pwd)/build/WasmJspi
  just test-wasm {{ tests }}

# run test-runtime on wasm (simple - no browser needed)
# Usage: just test-runtime-wasm
test-runtime-wasm:
  #!/bin/bash
  set -e

  # Build test-runtime for wasm if needed
  if [ ! -f "build/Wasm/test-runtime.js" ]; then
    echo "Building test-runtime for wasm..."
    cmake --build build/Wasm --target test-runtime
  fi

  # Run with node
  node -e "const test = require('./build/Wasm/test-runtime.js'); test().then(() => process.exit(0)).catch(e => { console.error(e); process.exit(1); })"

# ==================== Zig Cross-Compilation ====================
# Uses Zig as a C/C++ cross-compiler with Rust musl support
# Common targets: aarch64-linux-musl, x86_64-linux-musl

# configure cmake for zig cross-compilation (headless build)
# Usage: just configure-zig aarch64-linux-musl
configure-zig target:
  #!/bin/bash
  set -e

  # Verify zig is available
  if ! command -v zig &> /dev/null; then
    echo "Error: zig not found in PATH"
    echo "Install from https://ziglang.org/download/"
    echo "Or set ZIG_PATH environment variable to the directory containing zig"
    exit 1
  fi

  echo "Using zig: $(which zig)"
  zig version

  # Setup Rust target
  export RUSTUP_TOOLCHAIN=$(cat rust.version)

  # Determine Rust target from Zig target
  case "{{ target }}" in
    aarch64-linux-musl)
      RUST_TARGET="aarch64-unknown-linux-musl"
      ;;
    x86_64-linux-musl)
      RUST_TARGET="x86_64-unknown-linux-musl"
      ;;
    aarch64-linux-gnu)
      RUST_TARGET="aarch64-unknown-linux-gnu"
      ;;
    x86_64-linux-gnu)
      RUST_TARGET="x86_64-unknown-linux-gnu"
      ;;
    *)
      echo "Warning: Unknown Rust target mapping for {{ target }}"
      echo "You may need to manually add the Rust target"
      ;;
  esac

  if [ -n "$RUST_TARGET" ]; then
    echo "Adding Rust target: $RUST_TARGET"
    rustup +$RUSTUP_TOOLCHAIN target add $RUST_TARGET || echo "Target may already be installed"
  fi

  # Headless build - disable graphics/audio modules that won't work cross-compiled
  # Also disable modules requiring OpenSSL (crypto, http, ssh, network) for now
  cmake -Bbuild/Zig-{{ target }} -GNinja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/Zig.cmake \
    -DZIG_TARGET={{ target }} \
    -DCMAKE_BUILD_TYPE=Release \
    -DRUST_BUILD_TYPE=Small \
    -DSHARDS_WITH_EVERYTHING=OFF \
    -DSHARDS_WITH_LANGFFI=ON \
    -DSHARDS_WITH_ASSERT=ON \
    -DSHARDS_WITH_BROTLI=ON \
    -DSHARDS_WITH_CHANNELS=ON \
    -DSHARDS_WITH_CORE=ON \
    -DSHARDS_WITH_DEBUG=ON \
    -DSHARDS_WITH_FS=ON \
    -DSHARDS_WITH_JSON=ON \
    -DSHARDS_WITH_OS=ON \
    -DSHARDS_WITH_RANDOM=ON \
    -DSHARDS_WITH_SNAPPY=ON \
    -DSHARDS_WITH_SQLITE=ON \
    -DSHARDS_WITH_TRACY=ON \
    -DSHARDS_WITH_MARKDOWN=ON \
    -DSHARDS_WITH_STRUCT=ON \
    -DSHARDS_WITH_REFLECTION=ON \
    -DSHARDS_WITH_BIGINT=ON \
    -DSHARDS_WITH_CSV=ON \
    -DSHARDS_WITH_RUN=ON

# build shards for zig target (configures first if needed)
# Usage: just build-zig aarch64-linux-musl
build-zig target: (configure-zig target)
  cmake --build build/Zig-{{ target }} --target shards

# quick build zig (skips configure if already done)
# Usage: just build-zig-quick aarch64-linux-musl
build-zig-quick target:
  cmake --build build/Zig-{{ target }} --target shards

# strip zig-built binary (removes debug info, ~10x smaller)
# Usage: just strip-zig aarch64-linux-musl
strip-zig target:
  #!/bin/bash
  set -e
  BINARY="build/Zig-{{ target }}/shards"

  if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found. Run 'just build-zig {{ target }}' first."
    exit 1
  fi

  # macOS strip can't handle ELF, need llvm-strip
  if command -v llvm-strip &> /dev/null; then
    STRIP_CMD="llvm-strip"
  elif [ -f "/opt/homebrew/opt/llvm/bin/llvm-strip" ]; then
    STRIP_CMD="/opt/homebrew/opt/llvm/bin/llvm-strip"
  elif [ -f "/opt/homebrew/Cellar/llvm@20/20.1.8/bin/llvm-strip" ]; then
    STRIP_CMD="/opt/homebrew/Cellar/llvm@20/20.1.8/bin/llvm-strip"
  else
    echo "Error: llvm-strip not found. Install with: brew install llvm"
    exit 1
  fi

  SIZE_BEFORE=$(ls -lh "$BINARY" | awk '{print $5}')
  $STRIP_CMD "$BINARY"
  SIZE_AFTER=$(ls -lh "$BINARY" | awk '{print $5}')
  echo "Stripped $BINARY: $SIZE_BEFORE -> $SIZE_AFTER"

# build and strip zig target
# Usage: just build-zig-release aarch64-linux-musl
build-zig-release target: (build-zig target) (strip-zig target)
