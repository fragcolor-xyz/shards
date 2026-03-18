#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright © 2024 Fragcolor Pte. Ltd.
#
# Headless desktop test — fully automated, no human interaction needed.
# Starts a headless sway compositor + portal in an isolated D-Bus session,
# runs capture & input tests, then tears everything down.
#
# Requirements: sway, xdg-desktop-portal, xdg-desktop-portal-wlr, pipewire
# Install:  sudo pacman -S sway xdg-desktop-portal xdg-desktop-portal-wlr
# Optional: /dev/uinput access for input injection tests
#
# Usage: ./test-desktop-headless.sh [path/to/shards]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHARDS_BIN="${1:-${SCRIPT_DIR}/../../build/Debug/shards}"

if [ ! -x "$SHARDS_BIN" ]; then
  echo "ERROR: shards binary not found at $SHARDS_BIN" >&2
  echo "Usage: $0 [path/to/shards]" >&2
  exit 1
fi

# If we're already inside the isolated session, run the actual test
if [ "$_SHARDS_HEADLESS_INNER" = "1" ]; then
  exec "$SCRIPT_DIR/_test-desktop-headless-inner.sh" "$SHARDS_BIN"
fi

# Otherwise, launch an isolated D-Bus session so our portal doesn't
# conflict with the user's existing desktop portal (e.g., portal-hyprland).
echo "=== Launching isolated D-Bus session ==="
export _SHARDS_HEADLESS_INNER=1
exec dbus-run-session -- bash "$0" "$SHARDS_BIN"
