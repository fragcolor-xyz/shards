#!/bin/bash
# Baseline benchmark: the cost of N identical wire instances in Shards 1.x.
# Runs bench-instances.shs for several instance counts and reports spawn time,
# time until every instance has run its first activation, resident memory, and
# (optionally) how many times the entity wire was composed.
#
# This is the CPU-only 1.x baseline for the Shards 2.0 prototype, see
# docs/shards-2-compose-split.md §5. Use a Release build for real numbers.
#
# Usage: bench-instances.sh [path/to/shards] [counts...]
#   default binary: build/Release/shards; default counts: 0 100 1000
# Env:   RUNS=3          runs per count (each run is printed)
#        COMPOSE_COUNT=1 also count entity composes from a trace run (needs a
#                        build with trace logging; prints n/a otherwise)
# Works on macOS and Linux.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SHARDS_BIN="${1:-$REPO_DIR/build/Release/shards}"
shift || true
COUNTS=("$@")
[ ${#COUNTS[@]} -eq 0 ] && COUNTS=(0 100 1000)
RUNS="${RUNS:-3}"
BENCH="$SCRIPT_DIR/bench-instances.shs"

if [ ! -x "$SHARDS_BIN" ]; then
  echo "ERROR: shards binary not found at $SHARDS_BIN" >&2
  exit 1
fi

TMP="$(mktemp -d "${TMPDIR:-/tmp}/shards-bench.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

# One run: prints "spawn_ms ready_ms rss_kb"
run_once() {
  local n="$1" log="$TMP/run.log"
  "$SHARDS_BIN" "$BENCH" instances:"$n" hold:2.0 > "$log" 2>&1 &
  local pid=$!
  # Wait for READY (all instances alive and activated), then sample memory
  local waited=0
  until grep -q "READY" "$log" 2> /dev/null; do
    if ! kill -0 "$pid" 2> /dev/null; then
      echo "ERROR: run with $n instances exited early:" >&2
      tail -20 "$log" >&2
      return 1
    fi
    sleep 0.05
    waited=$((waited + 1))
    if [ $waited -gt 2400 ]; then
      echo "ERROR: timed out waiting for $n instances" >&2
      kill "$pid" 2> /dev/null
      return 1
    fi
  done
  local rss
  rss=$(ps -o rss= -p "$pid" | tr -d ' ')
  wait "$pid" || { echo "ERROR: run with $n instances failed:" >&2; tail -20 "$log" >&2; return 1; }
  local line spawn ready
  line=$(grep -o "BENCH .*" "$log")
  spawn=$(echo "$line" | sed -E 's/.*spawn_ms=([0-9.e+-]+).*/\1/')
  ready=$(echo "$line" | sed -E 's/.*ready_ms=([0-9.e+-]+).*/\1/')
  echo "$spawn $ready $rss"
}

echo "binary: $SHARDS_BIN"
echo "runs per count: $RUNS"
echo ""
printf "%10s %4s %12s %12s %12s %16s %16s\n" instances run spawn_ms ready_ms rss_kb ready_ms/inst rss_kb/inst
base_rss=""
status=0
for n in "${COUNTS[@]}"; do
  for r in $(seq 1 "$RUNS"); do
    if ! out=$(run_once "$n"); then
      status=1
      continue
    fi
    read -r spawn ready rss <<< "$out"
    if [ "$n" -eq 0 ]; then
      [ -z "$base_rss" ] && base_rss=$rss
      per_ready="-"
      per_rss="-"
    else
      per_ready=$(awk -v t="$ready" -v n="$n" 'BEGIN { printf "%.4f", t / n }')
      if [ -n "$base_rss" ]; then
        per_rss=$(awk -v r="$rss" -v b="$base_rss" -v n="$n" 'BEGIN { printf "%.1f", (r - b) / n }')
      else
        per_rss="-"
      fi
    fi
    printf "%10s %4s %12.3f %12.3f %12s %16s %16s\n" "$n" "$r" "$spawn" "$ready" "$rss" "$per_ready" "$per_rss"
  done
done
[ -z "$base_rss" ] && echo "(include count 0 to get per-instance memory)"

if [ "${COMPOSE_COUNT:-0}" = "1" ]; then
  n=10
  LOG_shards=trace "$SHARDS_BIN" "$BENCH" instances:$n hold:0.0 > "$TMP/trace.log" 2>&1
  # Top-level compose of each spawned copy logs "Composing wire: entity-<i>, "
  composes=$(grep -cE "Composing wire: entity-[0-9]+, $" "$TMP/trace.log")
  echo ""
  if [ "$composes" -eq 0 ]; then
    echo "entity composes for $n instances: n/a (no trace output from this build)"
  else
    echo "entity composes for $n instances: $composes"
  fi
fi

exit $status
