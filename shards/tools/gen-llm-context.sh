#!/usr/bin/env bash
# Generate the canonical Shards LLM context pack:
#   header (how-to + drill-down protocol) + language guide + live shard index.
#
# The shard index is produced from the binary (`shards enumerate`), so it reflects
# the actual runtime. NOTE: the shard set is platform/build-config dependent, so the
# committed snapshot is a convenience seed — the GROUND TRUTH is the live commands
# (`shards enumerate`, `shards docs <name> --json`). Refresh with `just gen-llm-context`.
#
# Usage: shards/tools/gen-llm-context.sh   (set SHARDS=/path/to/shards to override)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

BIN="${SHARDS:-}"
if [[ -z "$BIN" ]]; then
  if [[ -x "$ROOT/build/Release/shards" ]]; then BIN="$ROOT/build/Release/shards"
  elif [[ -x "$ROOT/build/shards" ]]; then BIN="$ROOT/build/shards"
  elif [[ -x "$ROOT/build/Debug/shards" ]]; then BIN="$ROOT/build/Debug/shards"
  else echo "ERROR: no shards binary found (build it, or set SHARDS=...)" >&2; exit 2; fi
fi

GUIDE="$ROOT/lib/shards-guide.md"
[[ -f "$GUIDE" ]] || { echo "ERROR: missing $GUIDE" >&2; exit 2; }

cat <<'EOF'
# Shards — LLM context pack

The canonical, always-loaded context for writing Shards. It is intentionally compact:
the language guide plus a one-line index of every shard. Expand any shard on demand
with the CLI instead of carrying full signatures here.

Agent loop:
  1. Discover    — `shards search <query>` or `shards enumerate --filter <substr>`
  2. Drill down  — `shards docs <name> --json`   (full params, types, defaults; ground truth)
  3. Write the .shs
  4. Verify      — `shards check <file> --json`   (parse + compose, never runs; structured errors)

The "Shard index" below is generated from the binary's introspection. It is a snapshot;
the live `enumerate`/`docs --json` commands are authoritative (the shard set varies by
platform/build).

EOF

echo "## Language guide"
echo
cat "$GUIDE"
echo
echo "## Shard index"
echo
echo '```'
"$BIN" enumerate 2>/dev/null
echo '```'
