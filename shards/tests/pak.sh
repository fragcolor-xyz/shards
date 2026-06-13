#!/usr/bin/env bash
#
# Smoke-test `shards pak` (single-file packaging) on the current platform.
#
# Packs a script (which also embeds a resource via @read), then runs the produced
# standalone binary from a clean directory — with neither the source script nor
# the resource file present — and verifies:
#   * the embedded script executes (marker on stdout),
#   * a compile-time @read resource is baked in (correct byte count),
#   * extra argv does not break execution,
#   * (macOS) the produced Mach-O has a valid code signature.
#
# Usage: shards/tests/pak.sh [path-to-shards]   (default: ./shards)
# Runnable locally and from CI (Windows via Git Bash; `.exe` is auto-resolved).

set -euo pipefail

SHARDS="${1:-./shards}"
# Resolve `.exe` on Windows/Git Bash, then make it an absolute path.
if [ ! -e "$SHARDS" ] && [ -e "${SHARDS}.exe" ]; then
  SHARDS="${SHARDS}.exe"
fi
if [ ! -e "$SHARDS" ]; then
  echo "pak.sh: shards binary not found: $SHARDS" >&2
  exit 1
fi
SHARDS="$(cd "$(dirname "$SHARDS")" && pwd)/$(basename "$SHARDS")"
chmod +x "$SHARDS" 2>/dev/null || true

WORK="$(mktemp -d 2>/dev/null || mktemp -d -t pak)"
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

MARKER="PAK_SMOKE_OK_42"

# A binary resource to embed at compile time via @read.
printf 'shards-pak-embedded-resource-payload' > asset.bin   # 36 bytes
ASSET_LEN="$(wc -c < asset.bin | tr -d '[:space:]')"

cat > app.shs <<EOF
@wire(main {
  @read("asset.bin" Bytes: true) | Count | Log("asset-bytes")
  "${MARKER}" | Log
} Looped: false)
@mesh(root)
@schedule(root main)
@run(root)
EOF

echo "== packing =="
# Default output naming: `app` (Unix) / `app.exe` (Windows) from the script stem.
"$SHARDS" pak app.shs

APP="app"
[ -e "app.exe" ] && APP="app.exe"
[ -e "$APP" ] || { echo "pak.sh: packed binary '$APP' was not produced" >&2; exit 1; }

# Run from a pristine directory that contains neither app.shs nor asset.bin,
# proving the single binary is self-contained.
mkdir run
cp "$APP" run/
(
  cd run
  echo "== running packed binary (no source/resource present) =="
  OUT="$("./$APP" extra:ignored 2>&1 || true)"
  echo "$OUT"

  echo "$OUT" | grep -q "$MARKER" \
    || { echo "FAIL: marker '$MARKER' not found in output" >&2; exit 1; }
  echo "$OUT" | grep -q "asset-bytes: ${ASSET_LEN}" \
    || { echo "FAIL: embedded @read resource missing (expected ${ASSET_LEN} bytes)" >&2; exit 1; }

  # macOS: the produced binary must be a validly signed Mach-O.
  if [ "$(uname -s)" = "Darwin" ]; then
    echo "== verifying code signature =="
    codesign --verify --verbose "$APP" \
      || { echo "FAIL: codesign --verify rejected the packed binary" >&2; exit 1; }
  fi
)

echo "PASS: shards pak smoke test"
