# Desktop Module

## Overview

Platform-specific desktop automation module providing screen capture, input injection, and window management.

## Platform Implementations

### Windows (`desktop.win.cpp`, `desktop.capture.win.hpp`)
- Uses Win32 APIs: FindWindow, SendInput, DXGI Desktop Duplication
- Window handle (`HWND`) is the central object, wrapped as `SHType::Object` with `windowCC = 'hwnd'`
- Old-style parameter handling (manual `setParam`/`getParam` switch, `ParamsInfo`)
- Links: DXGI, D3D11, ntdll

### Linux/Wayland (`desktop.linux.cpp`, `desktop.portal.linux.hpp`, `desktop.capture.linux.hpp`)
- Uses xdg-desktop-portal (RemoteDesktop + ScreenCast) via GLib/GIO D-Bus
- PipeWire for screen capture streams
- Portal session pointer wrapped as same `SHType::Object` with `windowCC`
- Modern PARAM macros (`PARAM_PARAMVAR`, `PARAM_IMPL`)
- Links: gio-2.0, gio-unix-2.0, libpipewire-0.3
- **Cannot be compiled on macOS** — Linux-only headers (`<gio/gio.h>`, `<pipewire/pipewire.h>`, `<linux/input-event-codes.h>`)
- LSP diagnostics about missing headers and `BTN_LEFT`/`BTN_RIGHT`/`BTN_MIDDLE` are expected on non-Linux

## Architecture

### Shared (`desktop.hpp`)
- `Desktop::Globals` — defines `windowType` object type used by both platforms
- Base classes: `WindowBase<T>`, `ActiveBase`, `PIDBase`, `WinOpBase`, `SizeBase`, etc.
- These base classes use old-style params; Linux impl doesn't inherit from them (uses PARAM macros instead)

### Linux Portal Flow
1. `Desktop.StartSession` → CreateSession → SelectDevices → SelectSources → Start (shows consent dialog)
2. Portal response provides PipeWire fd + node ID
3. `PipeWireCapture` connects a `pw_stream` to that node for frame capture
4. Input injection via D-Bus calls: `NotifyKeyboardKeycode`, `NotifyPointerMotionAbsolute`, `NotifyPointerButton`, etc.
5. `PortalSession::poll()` pumps GLib main context — must be called while waiting for async D-Bus responses

### Linux Shards
| Shard | Input | Output | Description |
|---|---|---|---|
| Desktop.StartSession | None | Session object | Opens portal with user consent |
| Desktop.CaptureFrame | Session | Session | Swaps PipeWire capture buffer |
| Desktop.Pixel | Int2 [x,y] | Color | Single pixel from capture (Session param) |
| Desktop.Pixels | Int4 [l,t,r,b] | Image | Region from capture (Session param) |
| Desktop.SendKeyEvent | Int2 [state,keycode] | Int2 | Keyboard event (Session param) |
| Desktop.SetMousePos | Int2 [x,y] | Int2 | Absolute pointer (Session param) |
| Desktop.SetMouseRelativePos | Int2 [dx,dy] | Int2 | Relative pointer (Session param) |
| Desktop.LeftClick | Int2 [x,y] | Int2 | Left click at position (Session param) |
| Desktop.RightClick | Int2 [x,y] | Int2 | Right click (Session param) |
| Desktop.MiddleClick | Int2 [x,y] | Int2 | Middle click (Session param) |
| Desktop.ScrollVertical | Float | Float | Vertical scroll (Session param) |
| Desktop.ScrollHorizontal | Float | Float | Horizontal scroll (Session param) |

## Build

- Windows: `if(WIN32)` in CMakeLists.txt
- Linux: `elseif(DESKTOP_LINUX)` — requires `gio-2.0`, `gio-unix-2.0`, `libpipewire-0.3` via pkg-config
- Module is marked `EXPERIMENTAL` on both platforms

## Testing

Must be tested on a Linux system with:
- A running Wayland compositor
- xdg-desktop-portal and a portal backend (e.g., xdg-desktop-portal-gnome, xdg-desktop-portal-wlr)
- PipeWire running

The portal consent dialog will appear on first `Desktop.StartSession` call.
