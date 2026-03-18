#!/bin/bash
# Inner script — runs inside an isolated D-Bus session.
# Called by test-desktop-headless.sh, not meant to be run directly.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHARDS_BIN="$1"
TMPDIR=$(mktemp -d /tmp/shards-headless-test.XXXXXX)

PIDS=()
cleanup() {
  echo "=== Cleaning up ==="
  for pid in "${PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  rm -rf "$TMPDIR"
}
trap cleanup EXIT

# --- Sway config: minimal headless ---
cat > "$TMPDIR/sway-config" << 'EOF'
output HEADLESS-1 resolution 1280x720
output * bg #204a4a solid_color
EOF

# --- Portal-wlr config: auto-select screen (no chooser dialog) ---
mkdir -p "$TMPDIR/xdg-desktop-portal-wlr"
cat > "$TMPDIR/xdg-desktop-portal-wlr/config" << 'EOF'
[screencast]
chooser_type = none
EOF

echo "=== Starting headless sway ==="
export XDG_CONFIG_HOME="$TMPDIR"
export XDG_CURRENT_DESKTOP=sway

WLR_BACKENDS=headless \
WLR_LIBINPUT_NO_DEVICES=1 \
  sway -c "$TMPDIR/sway-config" &>"$TMPDIR/sway.log" &
PIDS+=($!)
SWAY_PID=$!

# Wait for sway socket
for i in $(seq 1 50); do
  SWAYSOCK=$(find /run/user/$(id -u)/ -name "sway-ipc.*.sock" -newer "$TMPDIR/sway-config" 2>/dev/null | head -1)
  if [ -n "$SWAYSOCK" ]; then break; fi
  sleep 0.1
done

if [ -z "$SWAYSOCK" ]; then
  echo "ERROR: sway failed to start" >&2
  cat "$TMPDIR/sway.log" >&2
  exit 1
fi

export SWAYSOCK

# Find wayland display created by this sway instance
WAYLAND_DISPLAY=$(find /run/user/$(id -u)/ -name "wayland-*" -newer "$TMPDIR/sway-config" ! -name "*.lock" 2>/dev/null | head -1 | xargs basename 2>/dev/null)
if [ -z "$WAYLAND_DISPLAY" ]; then
  WAYLAND_DISPLAY=$(grep -o 'wayland-[0-9]*' "$TMPDIR/sway.log" | head -1)
fi
if [ -z "$WAYLAND_DISPLAY" ]; then
  echo "ERROR: Could not determine WAYLAND_DISPLAY" >&2
  exit 1
fi
export WAYLAND_DISPLAY

echo "  sway PID=$SWAY_PID, WAYLAND_DISPLAY=$WAYLAND_DISPLAY"

echo "=== Starting portal backend (xdg-desktop-portal-wlr) ==="
/usr/lib/xdg-desktop-portal-wlr &>"$TMPDIR/portal-wlr.log" &
PIDS+=($!)
sleep 0.5

echo "=== Starting portal frontend (xdg-desktop-portal) ==="
# Must start manually so it inherits XDG_CURRENT_DESKTOP=sway for backend routing.
# D-Bus auto-activation doesn't inherit our environment.
/usr/lib/xdg-desktop-portal &>"$TMPDIR/portal.log" &
PIDS+=($!)
sleep 1

# Clean up any leftover test files
rm -f /tmp/shards-test.txt /tmp/shards-exec-test.txt

echo "=== Running headless desktop test ==="
"$SHARDS_BIN" run "$SCRIPT_DIR/desktop-headless.shs"

echo "=== Headless desktop test completed successfully ==="
