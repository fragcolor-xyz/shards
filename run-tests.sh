#!/bin/bash

# Parallel GPU test runner with error detection and logging
# Optimized for automated error detection

# Parse command line options
SKIP_GFX=false
SKIP_UI=false
SKIP_AUDIO=false
SKIP_MISC=false
SKIP_SAMPLES=false
SKIP_PYTHON=false
WITH_CPU=false
VERBOSE=false
SHARDS_BIN="shards"

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-gfx)
            SKIP_GFX=true
            shift
            ;;
        --skip-ui)
            SKIP_UI=true
            shift
            ;;
        --skip-audio)
            SKIP_AUDIO=true
            shift
            ;;
        --skip-misc)
            SKIP_MISC=true
            shift
            ;;
        --skip-samples)
            SKIP_SAMPLES=true
            shift
            ;;
        --skip-python)
            SKIP_PYTHON=true
            shift
            ;;
        --with-cpu)
            WITH_CPU=true
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --shards-bin)
            SHARDS_BIN="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --skip-gfx       Skip graphics tests (gfx*.shs)"
            echo "  --skip-ui        Skip UI tests (ui-*.shs, egui-*.shs, input.shs)"
            echo "  --skip-audio     Skip audio tests"
            echo "  --skip-misc      Skip miscellaneous tests (ml, physics, crdts, etc.)"
            echo "  --skip-samples   Skip sample tests"
            echo "  --with-cpu       Include CPU-only tests from CI (general, strings, network, etc.)"
            echo "  --shards-bin     Path to shards binary (default: shards)"
            echo "  --verbose, -v    Show full commands being run"
            echo "  --help, -h       Show this help message"
            echo ""
            echo "Example: $0 --skip-audio --skip-samples"
            echo "Example: $0 --shards-bin build/Debug/shards"
            echo "Example: $0 --with-cpu   # Run all CI tests including CPU-only"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Convert SHARDS_BIN to absolute path if it's a relative path
if [[ "$SHARDS_BIN" != /* ]] && [[ "$SHARDS_BIN" != "shards" ]]; then
    SHARDS_BIN="$(pwd)/$SHARDS_BIN"
elif [[ "$SHARDS_BIN" == "shards" ]]; then
    # If it's just "shards", try to find the full path
    SHARDS_BIN="$(command -v shards || echo shards)"
fi

export LOG_GFX=debug
export RUST_BACKTRACE=full

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create logs directory (use absolute path)
LOG_DIR="$(pwd)/test-logs-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$LOG_DIR"

# Track results
FAILED_TESTS=()
PASSED_TESTS=()
TOTAL_TESTS=0

# Function to run a single test
run_test() {
    local test_file="$1"
    local test_name="${4:-$(basename "$test_file" .shs)}"
    local log_name=$(echo "$test_name" | tr '/' '-')
    local log_file="$LOG_DIR/${log_name}.log"
    local extra_args="${2:-}"
    local env_vars="${3:-}"

    if [ "$VERBOSE" = true ]; then
        if [ -n "$env_vars" ]; then
            echo "Running: $env_vars $SHARDS_BIN \"$test_file\" $extra_args"
        else
            echo "Running: $SHARDS_BIN \"$test_file\" $extra_args"
        fi
    fi

    if [ -n "$env_vars" ]; then
        eval "$env_vars $SHARDS_BIN \"$test_file\" $extra_args" > "$log_file" 2>&1
    else
        $SHARDS_BIN "$test_file" $extra_args > "$log_file" 2>&1
    fi

    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        echo "$test_name" >> "$LOG_DIR/passed.txt"
        return 0
    else
        echo -e "${RED}✗${NC} $test_name (exit code: $exit_code)"
        echo "$test_name" >> "$LOG_DIR/failed.txt"
        # Extract error context from log
        echo "--- Error in $test_name ---" >> "$LOG_DIR/all_errors.txt"
        tail -n 50 "$log_file" >> "$LOG_DIR/all_errors.txt"
        echo "" >> "$LOG_DIR/all_errors.txt"
        return 1
    fi
}

# Export function for parallel execution
export -f run_test
export LOG_DIR RED GREEN YELLOW NC VERBOSE SHARDS_BIN

echo "========================================"
echo "Starting GPU Tests (Parallel)"
echo "Logs directory: $LOG_DIR"
echo "========================================"
echo ""

# Initialize results files
touch "$LOG_DIR/passed.txt" "$LOG_DIR/failed.txt" "$LOG_DIR/all_errors.txt"

# Collect all tests to run
declare -a TESTS=()

# Graphics test scripts
if [ "$SKIP_GFX" = false ]; then
    for i in $(find shards/tests -maxdepth 1 -name 'gfx*.shs'); do
        TESTS+=("$i||")
    done
fi

# UI tests
if [ "$SKIP_UI" = false ]; then
    TESTS+=(
        "shards/tests/input.shs||"
        "shards/tests/ui-0.shs||"
        "shards/tests/ui-1.shs||"
        "shards/tests/ui-2.shs||"
        "shards/tests/ui-nested.shs||"
        "shards/tests/egui-demo.shs||"
        "shards/tests/egui-plot.shs||"
        "shards/tests/ui-drag-and-drop.shs||"
        "shards/tests/ui-selectable-drag.shs||"
    )
fi

# Audio tests
if [ "$SKIP_AUDIO" = false ]; then
    TESTS+=(
        "shards/tests/audio.shs|test-device:true|"
        "shards/tests/audio2.shs|test-device:true|"
    )
fi

# Miscellaneous tests
if [ "$SKIP_MISC" = false ]; then
    TESTS+=(
        "shards/tests/ml.shs||"
        "shards/tests/fib.shs||LOG_shards=trace"
        "shards/tests/ml-test.shs||"
        "shards/tests/global-init.shs||"
        "shards/tests/hot-reload.shs||"
        "shards/tests/crdts.shs||"
        "shards/tests/crdt-benchmarks.shs||"
        "shards/tests/physics.shs||"
    )
fi

# CPU-only tests (from CI build-linux.yml and build-macos.yml)
if [ "$WITH_CPU" = true ]; then
    TESTS+=(
        # Core language tests
        "shards/tests/hello.shs||"
        "shards/tests/general.shs||"
        "shards/tests/zip-map.shs||"
        "shards/tests/strings.shs||"
        "shards/tests/table-compose.shs||"
        "shards/tests/variables.shs||"
        "shards/tests/subwires.shs||"
        "shards/tests/linalg.shs||"
        "shards/tests/builtins.shs||"
        "shards/tests/struct.shs||"
        "shards/tests/flows.shs||"
        "shards/tests/channels.shs||"
        "shards/tests/expect.shs||"
        "shards/tests/failures.shs||"
        "shards/tests/wire-macro.shs||"
        "shards/tests/const-vars.shs||"
        "shards/tests/branch.shs||"
        "shards/tests/take.shs||"
        "shards/tests/casting-numbers.shs||"
        "shards/tests/pure.shs||"
        "shards/tests/events.shs||"
        "shards/tests/tablecase.shs||"
        "shards/tests/types.shs||"
        "shards/tests/return.shs||"
        "shards/tests/table-seq-push.shs||"
        "shards/tests/traits.shs||"
        "shards/tests/shards.shs||"
        "shards/tests/table-recurse.shs||"
        "shards/tests/whendone.shs||"
        "shards/tests/help.shs||"
        "shards/tests/suspend-resume.shs||"
        "shards/tests/complex-deserialize.shs||"
        # Network and I/O tests
        "shards/tests/network.shs||"
        "shards/tests/network-ws.shs||"
        "shards/tests/fs-security.shs||"
        "shards/tests/fileops.shs||"
        "shards/tests/http.shs||"
        "shards/tests/imaging.shs||"
        "shards/tests/localshell.shs||"
        # Compression and encoding tests
        "shards/tests/bigint.shs||"
        "shards/tests/brotli.shs||"
        "shards/tests/snappy.shs||"
        "shards/tests/crypto.shs||"
        "shards/tests/rust.shs||"
        # Database tests
        "shards/tests/db.shs|with-sqlite-vec:true|"
        "shards/tests/db-paths.shs||"
        # Misc tests
        "shards/tests/markdown.shs||"
        "shards/tests/jinja.shs||"
        "shards/tests/llm.shs||"
        "shards/tests/llm-mistral.shs||"
        "shards/tests/llm-gguf.shs||"
        "shards/tests/llm-embed.shs||"
    )
fi

TOTAL_TESTS=${#TESTS[@]}

# Run tests in parallel (4 at a time to avoid overwhelming the GPU)
echo "Running $TOTAL_TESTS tests (4 parallel jobs)..."
echo ""

for test_spec in "${TESTS[@]}"; do
    IFS='|' read -r test_file args env_vars <<< "$test_spec"
    run_test "$test_file" "$args" "$env_vars" &

    # Limit to 4 parallel jobs
    while [ $(jobs -r | wc -l) -ge 4 ]; do
        sleep 0.1
    done
done

# Wait for all background jobs to complete
wait

if [ "$SKIP_SAMPLES" = false ]; then
    echo ""
    echo "========================================"
    echo "Running Samples"
    echo "========================================"
    echo ""

    # Change to docs/samples directory for samples
    pushd docs/samples > /dev/null

    # UI/GFX samples (looped)
    for i in $(find shards -name '*.shs' \( -path '*UI*' -or -path '*GFX*' \)); do
        sample_name=$(echo "$i" | sed 's|shards/||' | sed 's|\.shs$||')
        run_test "run-sample.shs" "looped:true file:\"$i\"" "" "$sample_name" &

        # Limit to 4 parallel jobs
        while [ $(jobs -r | wc -l) -ge 4 ]; do
            sleep 0.1
        done
    done

    # Other samples (not looped)
    for i in $(find shards -name '*.shs' \( ! -path '*UI*' ! -path '*GFX*' ! -path '*Dialog*' \)); do
        sample_name=$(echo "$i" | sed 's|shards/||' | sed 's|\.shs$||')
        run_test "run-sample.shs" "file:\"$i\" looped:false" "" "$sample_name" &

        # Limit to 4 parallel jobs
        while [ $(jobs -r | wc -l) -ge 4 ]; do
            sleep 0.1
        done
    done

    # Wait for all sample tests to complete
    wait

    # Return to original directory
    popd > /dev/null
fi

echo ""
echo "========================================"
echo "Test Results Summary"
echo "========================================"

PASSED_COUNT=$(wc -l < "$LOG_DIR/passed.txt" | tr -d ' ')
FAILED_COUNT=$(wc -l < "$LOG_DIR/failed.txt" | tr -d ' ')

echo -e "${GREEN}Passed:${NC} $PASSED_COUNT / $TOTAL_TESTS"
echo -e "${RED}Failed:${NC} $FAILED_COUNT / $TOTAL_TESTS"
echo ""

if [ $FAILED_COUNT -gt 0 ]; then
    echo -e "${RED}Failed tests:${NC}"
    cat "$LOG_DIR/failed.txt" | while read test; do
        echo "  - $test"
    done
    echo ""
    echo "Error summary saved to: $LOG_DIR/all_errors.txt"
    echo "Individual logs in: $LOG_DIR/"
    echo ""

    # Show first error for quick diagnosis
    echo "========================================"
    echo "First Error Preview:"
    echo "========================================"
    head -n 30 "$LOG_DIR/all_errors.txt"
    echo ""
    echo "(See $LOG_DIR/all_errors.txt for all errors)"

    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
