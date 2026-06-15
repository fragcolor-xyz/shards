#!/usr/bin/env bash
# Eval harness for `shards check` — the measuring stick for diagnostic quality.
#
# For each script in cases/, runs `shards check --json` and asserts that the
# diagnostics carry what an agent repair loop needs: the right phase/kind, a
# precise location, structured actual/expected types, and the repair aids
# (type-directed `candidates`, edit-distance `did_you_mean`).
#
# Reports a pass/fail summary and a "repairability" score: the fraction of broken
# cases whose diagnostic included an actionable repair aid. This is exactly the
# signal roadmap items 3.3 (canonical-form benchmark) and 3.4 (corpus) build on.
#
# Usage: shards/tests/check/run.sh   (set SHARDS=/path/to/shards to override)
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
CASES="$HERE/cases"

# Locate the shards binary.
if [[ -n "${SHARDS:-}" ]]; then
  BIN="$SHARDS"
elif [[ -x "$ROOT/build/Release/shards" ]]; then
  BIN="$ROOT/build/Release/shards"
elif [[ -x "$ROOT/build/Debug/shards" ]]; then
  BIN="$ROOT/build/Debug/shards"
else
  echo "ERROR: could not find a shards binary (build it, or set SHARDS=...)" >&2
  exit 2
fi

command -v jq >/dev/null 2>&1 || { echo "ERROR: jq is required" >&2; exit 2; }

echo "Using binary: $BIN"
echo

PASS=0
FAIL=0
REPAIR_OK=0
REPAIR_TOTAL=0

# assert <description> <condition-exit-code>
assert() {
  local desc="$1" rc="$2"
  if [[ "$rc" -eq 0 ]]; then
    echo "  PASS: $desc"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: $desc"
    FAIL=$((FAIL + 1))
  fi
}

# jq_true <json> <filter>  -> returns 0 if filter is truthy
jq_true() { echo "$1" | jq -e "$2" >/dev/null 2>&1; }

run_case() {
  local name="$1"
  local file="$CASES/$name.shs"
  OUT="$("$BIN" check --json "$file" 2>/dev/null)"
  CODE=$?
  echo "case: $name (exit $CODE)"
}

# ---- ok-basic: valid script -------------------------------------------------
run_case ok-basic
assert "exit code 0" "$([[ $CODE -eq 0 ]]; echo $?)"
assert "ok == true" "$(jq_true "$OUT" '.ok == true'; echo $?)"
assert "no diagnostics" "$(jq_true "$OUT" '(.diagnostics | length) == 0'; echo $?)"
echo

# ---- type-mismatch: compose-phase input-type mismatch + candidates ----------
run_case type-mismatch
assert "exit code 1" "$([[ $CODE -eq 1 ]]; echo $?)"
assert "has compose input-type-mismatch" \
  "$(jq_true "$OUT" '[.diagnostics[] | select(.phase=="compose" and .kind=="input-type-mismatch")] | length > 0'; echo $?)"
assert "diagnostic carries structured actual+expected" \
  "$(jq_true "$OUT" '[.diagnostics[] | select(.kind=="input-type-mismatch") | select(.actual != null and (.expected|length)>0)] | length > 0'; echo $?)"
REPAIR_TOTAL=$((REPAIR_TOTAL + 1))
if jq_true "$OUT" '[.diagnostics[] | select(.kind=="input-type-mismatch") | select((.candidates|length)>0)] | length > 0'; then
  assert "type-directed candidates present" 0
  REPAIR_OK=$((REPAIR_OK + 1))
else
  assert "type-directed candidates present" 1
fi
echo

# ---- unknown-shard: construct-phase + did_you_mean --------------------------
run_case unknown-shard
assert "exit code 1" "$([[ $CODE -eq 1 ]]; echo $?)"
assert "has construct unknown-shard" \
  "$(jq_true "$OUT" '[.diagnostics[] | select(.phase=="construct" and .kind=="unknown-shard")] | length > 0'; echo $?)"
REPAIR_TOTAL=$((REPAIR_TOTAL + 1))
if jq_true "$OUT" '[.diagnostics[] | select(.kind=="unknown-shard") | select(.did_you_mean | index("Log"))] | length > 0'; then
  assert "did_you_mean includes Log" 0
  REPAIR_OK=$((REPAIR_OK + 1))
else
  assert "did_you_mean includes Log" 1
fi
echo

# ---- define: command-line defines reach compose (parity with `run`) --------
DEF_FILE="$CASES/define.shs"
# Without the define, @greeting is undefined -> construct error (exit 1).
OUT="$("$BIN" check --json "$DEF_FILE" 2>/dev/null)"; CODE=$?
echo "case: define (no define) (exit $CODE)"
assert "missing define -> exit 1" "$([[ $CODE -eq 1 ]]; echo $?)"
# Supplying it (exactly as `run` would) makes the script compose cleanly.
OUT="$("$BIN" check --json "$DEF_FILE" greeting:hello 2>/dev/null)"; CODE=$?
echo "case: define (greeting:hello) (exit $CODE)"
assert "supplied define -> exit 0" "$([[ $CODE -eq 0 ]]; echo $?)"
assert "ok == true with define" "$(jq_true "$OUT" '.ok == true'; echo $?)"
echo

# ---- include-resolution: relative -I resolves like `run` -------------------
# Regression guard: a script in cases/inc/sub/ includes "lib.shs", which lives in
# cases/inc/ (one level up) and is only reachable via -I. We invoke from cases/
# with a RELATIVE -Iinc. `check` used to canonicalize -I *after* setRootPath had
# already changed the cwd to the script's dir, so the relative -I pointed at the
# wrong place and the include silently failed. It must resolve against the
# invocation cwd, exactly as `run` does.
INC_OUT="$(cd "$CASES" && "$BIN" check --json -Iinc inc/sub/main.shs 2>/dev/null)"; INC_CODE=$?
echo "case: include-relative-I (exit $INC_CODE)"
assert "relative -I resolves include -> exit 0" "$([[ $INC_CODE -eq 0 ]]; echo $?)"
assert "ok == true with relative -I" "$(jq_true "$INC_OUT" '.ok == true'; echo $?)"
# Without -I the include is genuinely unresolvable -> a parse-phase diagnostic.
INC_OUT2="$(cd "$CASES" && "$BIN" check --json inc/sub/main.shs 2>/dev/null)"; INC_CODE2=$?
echo "case: include-no-I (exit $INC_CODE2)"
assert "missing -I -> exit 1" "$([[ $INC_CODE2 -eq 1 ]]; echo $?)"
assert "has parse diagnostic for unresolved include" \
  "$(jq_true "$INC_OUT2" '[.diagnostics[] | select(.phase=="parse")] | length > 0'; echo $?)"
# A bad -I must fail LOUDLY (exit 2), not be silently dropped — otherwise a mistyped
# include dir looks like "this -I wasn't honored" and surfaces as a confusing
# "include not found" later. stdout stays clean (error goes to stderr) for --json.
INC_OUT3="$(cd "$CASES" && "$BIN" check --json -Idoes-not-exist inc/sub/main.shs 2>/dev/null)"; INC_CODE3=$?
echo "case: include-bad-I (exit $INC_CODE3)"
assert "non-existent -I dir -> exit 2" "$([[ $INC_CODE3 -eq 2 ]]; echo $?)"
assert "bad -I keeps stdout clean (empty)" "$([[ -z "$INC_OUT3" ]]; echo $?)"
echo

# ---- syntax-error: parse-phase ---------------------------------------------
run_case syntax-error
assert "exit code 1" "$([[ $CODE -eq 1 ]]; echo $?)"
assert "has parse syntax diagnostic" \
  "$(jq_true "$OUT" '[.diagnostics[] | select(.phase=="parse" and .kind=="syntax")] | length > 0'; echo $?)"
assert "parse diagnostic has a location (line > 0)" \
  "$(jq_true "$OUT" '[.diagnostics[] | select(.phase=="parse" and .line > 0)] | length > 0'; echo $?)"
echo

# ---- summary ---------------------------------------------------------------
echo "================================================================"
echo "assertions: $PASS passed, $FAIL failed"
if [[ $REPAIR_TOTAL -gt 0 ]]; then
  echo "repairability: $REPAIR_OK/$REPAIR_TOTAL broken cases shipped an actionable repair aid"
fi
echo "================================================================"

[[ $FAIL -eq 0 ]]
