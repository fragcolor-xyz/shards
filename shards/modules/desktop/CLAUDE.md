# Desktop Module

## Overview

Platform-specific desktop automation module providing screen capture, input injection, and window management.

## Platform Implementations

### Windows (`desktop.win.cpp`, `desktop.capture.win.hpp`)
- Uses Win32 APIs: FindWindow, SendInput, DXGI Desktop Duplication
- Window handle (`HWND`) is the central object, wrapped as `SHType::Object` with `windowCC = 'hwnd'`
- Old-style parameter handling (manual `setParam`/`getParam` switch, `ParamsInfo`)
- Links: DXGI, D3D11, ntdll

### Linux/Wayland (`desktop.linux.cpp`, `desktop.portal.linux.hpp`, `desktop.capture.linux.hpp`, `desktop.uinput.linux.hpp`)
- **Screen capture**: xdg-desktop-portal (ScreenCast) via GLib/GIO D-Bus + PipeWire
- **Input injection**: `/dev/uinput` kernel virtual devices (compositor-independent)
- Portal session pointer wrapped as same `SHType::Object` with `windowCC`
- Modern PARAM macros (`PARAM_PARAMVAR`, `PARAM_IMPL`)
- Links: gio-2.0, gio-unix-2.0, libpipewire-0.3
- **Cannot be compiled on macOS** — Linux-only headers (`<gio/gio.h>`, `<pipewire/pipewire.h>`, `<linux/uinput.h>`)
- LSP diagnostics about missing headers and `BTN_LEFT`/`BTN_RIGHT`/`BTN_MIDDLE` are expected on non-Linux

## Architecture

### Shared (`desktop.hpp`)
- `Desktop::Globals` — defines `windowType` object type used by both platforms
- Base classes: `WindowBase<T>`, `ActiveBase`, `PIDBase`, `WinOpBase`, `SizeBase`, etc.
- These base classes use old-style params; Linux impl doesn't inherit from them (uses PARAM macros instead)

### Linux Portal Flow (ScreenCast with RemoteDesktop fallback)
1. `Desktop.StartSession` → Try `RemoteDesktop.CreateSession`
   - **If supported** (GNOME, KDE): `SelectDevices` → `SelectSources` → `RemoteDesktop.Start` (shows consent dialog)
   - **If not supported** (Hyprland, wlroots): Falls back to `ScreenCast.CreateSession` → `SelectSources` → `ScreenCast.Start`
2. Portal response provides PipeWire fd + node ID
3. `PipeWireCapture` connects a `pw_stream` to that node for frame capture
4. `PortalSession::poll()` pumps GLib main context — must be called while waiting for async D-Bus responses

### Linux Input Injection (UInput)
Input injection uses `/dev/uinput` kernel-level virtual devices instead of portal D-Bus methods.
This works on **all compositors** (Hyprland, GNOME, KDE, wlroots, etc.) without portal support.

- `UInputDevice` creates two virtual devices: keyboard + mouse
- Keyboard: `EV_KEY` events for all `KEY_*` codes
- Mouse: `EV_KEY` (buttons), `EV_REL` (motion + scroll), `EV_ABS` (absolute position, 0–32767 range)
- Absolute positioning scales coordinates using capture dimensions from PipeWireCapture
- Global singleton, initialized lazily on first input shard activation

**Required permissions**: User must have write access to `/dev/uinput`:
```bash
sudo usermod -aG input $USER
echo 'KERNEL=="uinput", GROUP="input", MODE="0660"' | sudo tee /etc/udev/rules.d/99-uinput.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
# Log out and back in for group change to take effect
```

### Linux Shards
| Shard | Input | Output | Description |
|---|---|---|---|
| Desktop.StartSession | None | Session object | Opens portal with user consent (RemoteDesktop → ScreenCast fallback) |
| Desktop.StartDirectCapture | Int (node ID) | Session object | Direct PipeWire capture — no portal, no consent dialog (headless/CI) |
| Desktop.CaptureFrame | Session | Session | Swaps PipeWire capture buffer |
| Desktop.Pixel | Int2 [x,y] | Color | Single pixel from capture (Session param) |
| Desktop.Pixels | Int4 [l,t,r,b] | Image | Region from capture (Session param) |
| Desktop.SendKeyEvent | Int2 [state,keycode] | Int2 | Keyboard event via uinput (Session param for compat) |
| Desktop.SetMousePos | Int2 [x,y] | Int2 | Absolute pointer via uinput (Session param for screen dims) |
| Desktop.SetMouseRelativePos | Int2 [dx,dy] | Int2 | Relative pointer via uinput (Session param for compat) |
| Desktop.LeftClick | Int2 [x,y] | Int2 | Left click at position via uinput (Session param for screen dims) |
| Desktop.RightClick | Int2 [x,y] | Int2 | Right click via uinput |
| Desktop.MiddleClick | Int2 [x,y] | Int2 | Middle click via uinput |
| Desktop.ScrollVertical | Float | Float | Vertical scroll via uinput |
| Desktop.ScrollHorizontal | Float | Float | Horizontal scroll via uinput |

## Build

- Windows: `if(WIN32)` in CMakeLists.txt
- Linux: `elseif(DESKTOP_LINUX)` — requires `gio-2.0`, `gio-unix-2.0`, `libpipewire-0.3` via pkg-config
- Module is marked `EXPERIMENTAL` on both platforms

## Testing

### Interactive (portal)
Must be tested on a Linux system with:
- A running Wayland compositor
- xdg-desktop-portal and a portal backend (e.g., xdg-desktop-portal-hyprland, xdg-desktop-portal-gnome, xdg-desktop-portal-wlr)
- PipeWire running
- `/dev/uinput` access for input injection (user in `input` group + udev rule)

The portal consent dialog will appear on first `Desktop.StartSession` call.

### Headless / CI (direct PipeWire)
`Desktop.StartDirectCapture` bypasses the portal entirely — no consent dialog, no D-Bus.
Requires a running Wayland compositor and PipeWire, but can be headless (e.g., `sway` or `weston --backend=headless`).

1. Start a headless compositor: `WLR_BACKENDS=headless sway` or `weston --backend=headless`
2. Find the PipeWire screen capture node ID: `pw-cli list-objects | grep -A5 'node.name'`
3. Pass node ID to `Desktop.StartDirectCapture`

This enables fully automated testing without human interaction.
