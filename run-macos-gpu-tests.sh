#!/bin/bash

# Parallel GPU test runner with error detection and logging
# Optimized for automated error detection

export LOG_GFX=debug
export RUST_BACKTRACE=full

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create logs directory
LOG_DIR="test-logs-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$LOG_DIR"

# Track results
FAILED_TESTS=()
PASSED_TESTS=()
TOTAL_TESTS=0

# Function to run a single test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .shs)
    local log_file="$LOG_DIR/${test_name}.log"
    local extra_args="${2:-}"
    local env_vars="${3:-}"

    echo "Running: $test_name"

    if [ -n "$env_vars" ]; then
        eval "$env_vars shards \"$test_file\" $extra_args" > "$log_file" 2>&1
    else
        shards "$test_file" $extra_args > "$log_file" 2>&1
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
export LOG_DIR RED GREEN YELLOW NC

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
for i in $(find shards/tests -maxdepth 1 -name 'gfx*.shs'); do
    TESTS+=("$i||")
done

# Standard tests
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
    "shards/tests/ml.shs||"
    "shards/tests/fib.shs||LOG_shards=trace"
    "lib/ml/test.shs||"
    "shards/tests/global-init.shs||"
    "shards/tests/hot-reload.shs||"
    "shards/tests/audio.shs|test-device:true|"
    "shards/tests/audio2.shs|test-device:true|"
    "shards/tests/crdts.shs||"
    "shards/tests/crdt-benchmarks.shs||"
    "shards/tests/tui.shs||"
    "shards/tests/tui1.shs||"
    "shards/tests/physics.shs||"
)

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
