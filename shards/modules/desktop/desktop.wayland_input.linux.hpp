/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SH_DESKTOP_WAYLAND_INPUT_LINUX
#define SH_DESKTOP_WAYLAND_INPUT_LINUX

#include <shards/log/log.hpp>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <time.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include "virtual-keyboard-unstable-v1-client-protocol.h"
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

namespace Desktop {

static inline shards::logging::Logger getWaylandInputLogger() {
  static auto logger = shards::logging::getOrCreate("Desktop.WaylandInput");
  return logger;
}

// Wayland-native virtual keyboard and pointer via compositor protocols.
// Works in headless compositors (sway, weston) without libinput/uinput.
// Uses zwp_virtual_keyboard_v1 for keyboard and zwlr_virtual_pointer_v1 for mouse.
//
// Uses the wtype approach: generates a custom XKB keymap that maps each needed
// linux keycode to a sequential XKB keycode with the correct keysym. This is
// necessary because the virtual keyboard protocol requires the client (terminal)
// to compile the keymap, and full evdev keymaps don't work reliably with all
// compositors. The keymap uses `include "complete"` for XKB types/compatibility.
class WaylandVirtualInput {
public:
  WaylandVirtualInput() = default;
  ~WaylandVirtualInput() { shutdown(); }

  WaylandVirtualInput(const WaylandVirtualInput &) = delete;
  WaylandVirtualInput &operator=(const WaylandVirtualInput &) = delete;

  bool init() {
    const char *waylandDisplay = getenv("WAYLAND_DISPLAY");
    SPDLOG_LOGGER_INFO(getWaylandInputLogger(), "Connecting to Wayland display: {}",
                       waylandDisplay ? waylandDisplay : "(default)");
    _display = wl_display_connect(nullptr);
    if (!_display) {
      SPDLOG_LOGGER_DEBUG(getWaylandInputLogger(), "Failed to connect to Wayland display");
      return false;
    }

    _registry = wl_display_get_registry(_display);
    wl_registry_add_listener(_registry, &registryListener, this);
    wl_display_dispatch(_display);
    wl_display_roundtrip(_display);

    if (!_seat) {
      SPDLOG_LOGGER_DEBUG(getWaylandInputLogger(), "No wl_seat found");
      shutdown();
      return false;
    }

    bool hasKeyboard = (_kbManager != nullptr);
    bool hasPointer = (_ptrManager != nullptr);

    if (!hasKeyboard && !hasPointer) {
      SPDLOG_LOGGER_DEBUG(getWaylandInputLogger(), "Neither virtual keyboard nor virtual pointer manager available");
      shutdown();
      return false;
    }

    if (hasKeyboard) {
      _keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(_kbManager, _seat);
      if (!_keyboard) {
        SPDLOG_LOGGER_WARN(getWaylandInputLogger(), "Failed to create virtual keyboard object");
        hasKeyboard = false;
      } else if (!setupKeymap()) {
        SPDLOG_LOGGER_WARN(getWaylandInputLogger(), "Failed to setup XKB keymap on virtual keyboard");
        zwp_virtual_keyboard_v1_destroy(_keyboard);
        _keyboard = nullptr;
        hasKeyboard = false;
      }
    }

    if (hasPointer) {
      _pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(_ptrManager, _seat);
      if (!_pointer) {
        SPDLOG_LOGGER_WARN(getWaylandInputLogger(), "Failed to create virtual pointer");
        hasPointer = false;
      }
    }

    wl_display_roundtrip(_display);

    _available = true;
    _hasKeyboard = hasKeyboard;
    _hasPointer = hasPointer;

    SPDLOG_LOGGER_INFO(getWaylandInputLogger(), "Wayland virtual input created (keyboard={}, pointer={})",
                       hasKeyboard ? "yes" : "no", hasPointer ? "yes" : "no");
    return true;
  }

  void shutdown() {
    _available = false;

    if (_keyboard) {
      zwp_virtual_keyboard_v1_destroy(_keyboard);
      _keyboard = nullptr;
    }
    if (_pointer) {
      zwlr_virtual_pointer_v1_destroy(_pointer);
      _pointer = nullptr;
    }
    if (_kbManager) {
      zwp_virtual_keyboard_manager_v1_destroy(_kbManager);
      _kbManager = nullptr;
    }
    if (_ptrManager) {
      zwlr_virtual_pointer_manager_v1_destroy(_ptrManager);
      _ptrManager = nullptr;
    }
    if (_seat) {
      wl_seat_destroy(_seat);
      _seat = nullptr;
    }
    if (_registry) {
      wl_registry_destroy(_registry);
      _registry = nullptr;
    }
    if (_display) {
      wl_display_disconnect(_display);
      _display = nullptr;
    }
  }

  bool isAvailable() const { return _available; }

  // Linux evdev keycode (e.g. KEY_A=30, KEY_ENTER=28)
  void keyboardKey(uint32_t linuxKeycode, bool pressed) {
    if (!_available || !_hasKeyboard)
      return;

    // Map linux keycode to our custom sequential keycode
    uint32_t customCode = _linuxToCustom[linuxKeycode < MAX_KEYS ? linuxKeycode : 0];
    if (customCode == 0) {
      SPDLOG_LOGGER_WARN(getWaylandInputLogger(), "Unmapped linux keycode {}", linuxKeycode);
      return;
    }

    SPDLOG_LOGGER_TRACE(getWaylandInputLogger(), "key {} (custom {}) {}", linuxKeycode, customCode, pressed ? "down" : "up");

    // Track modifier state and send explicit modifiers events
    // (virtual keyboards have update_state=false in wlroots)
    bool isModifier = false;
    uint32_t modBit = 0;
    switch (linuxKeycode) {
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      modBit = 1; // Shift
      isModifier = true;
      break;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      modBit = 4; // Control
      isModifier = true;
      break;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      modBit = 8; // Mod1 (Alt)
      isModifier = true;
      break;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
      modBit = 64; // Mod4 (Super)
      isModifier = true;
      break;
    }

    if (isModifier) {
      if (pressed)
        _modState |= modBit;
      else
        _modState &= ~modBit;
    }

    zwp_virtual_keyboard_v1_key(_keyboard, 0, customCode,
                                pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
    wl_display_roundtrip(_display);

    if (isModifier) {
      zwp_virtual_keyboard_v1_modifiers(_keyboard, _modState, 0, 0, 0);
      wl_display_roundtrip(_display);
    }
  }

  void pointerMotionRelative(int dx, int dy) {
    if (!_available || !_hasPointer)
      return;
    zwlr_virtual_pointer_v1_motion(_pointer, nowMs(), wl_fixed_from_int(dx), wl_fixed_from_int(dy));
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_roundtrip(_display);
  }

  void pointerMotionAbsolute(int x, int y, int screenW, int screenH) {
    if (!_available || !_hasPointer || screenW <= 0 || screenH <= 0)
      return;
    zwlr_virtual_pointer_v1_motion_absolute(_pointer, nowMs(), (uint32_t)x, (uint32_t)y, (uint32_t)screenW, (uint32_t)screenH);
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_roundtrip(_display);
  }

  void pointerButton(uint32_t button, bool pressed) {
    if (!_available || !_hasPointer)
      return;
    zwlr_virtual_pointer_v1_button(_pointer, nowMs(), button,
                                   pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_roundtrip(_display);
  }

  void scrollVertical(int amount) {
    if (!_available || !_hasPointer)
      return;
    zwlr_virtual_pointer_v1_axis_discrete(_pointer, nowMs(), WL_POINTER_AXIS_VERTICAL_SCROLL,
                                          wl_fixed_from_int(amount * 15), amount);
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_roundtrip(_display);
  }

  void scrollHorizontal(int amount) {
    if (!_available || !_hasPointer)
      return;
    zwlr_virtual_pointer_v1_axis_discrete(_pointer, nowMs(), WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                                          wl_fixed_from_int(amount * 15), amount);
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_roundtrip(_display);
  }

private:
  static uint32_t nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
  }

  // Table mapping linux keycodes to XKB keysym names.
  // Only keys with entries here are available for virtual keyboard input.
  struct KeyEntry {
    uint32_t linuxCode;
    const char *keysym;       // XKB keysym name (Level1)
    const char *shiftKeysym;  // Level2 keysym (or nullptr for ONE_LEVEL)
  };

  static constexpr int MAX_KEYS = 256;

  // wtype-style keymap: generate a custom XKB keymap that maps sequential
  // keycodes (starting from XKB 9 / evdev 1) to keysyms for each linux keycode.
  // Uses `include "complete"` for types/compatibility (required by compositors).
  bool setupKeymap() {
    // Map of linux keycodes → keysym names
    static const KeyEntry keyTable[] = {
        {KEY_ESC, "Escape", nullptr},
        {KEY_1, "1", "exclam"},
        {KEY_2, "2", "at"},
        {KEY_3, "3", "numbersign"},
        {KEY_4, "4", "dollar"},
        {KEY_5, "5", "percent"},
        {KEY_6, "6", "asciicircum"},
        {KEY_7, "7", "ampersand"},
        {KEY_8, "8", "asterisk"},
        {KEY_9, "9", "parenleft"},
        {KEY_0, "0", "parenright"},
        {KEY_MINUS, "minus", "underscore"},
        {KEY_EQUAL, "equal", "plus"},
        {KEY_BACKSPACE, "BackSpace", nullptr},
        {KEY_TAB, "Tab", nullptr},
        {KEY_Q, "q", "Q"},
        {KEY_W, "w", "W"},
        {KEY_E, "e", "E"},
        {KEY_R, "r", "R"},
        {KEY_T, "t", "T"},
        {KEY_Y, "y", "Y"},
        {KEY_U, "u", "U"},
        {KEY_I, "i", "I"},
        {KEY_O, "o", "O"},
        {KEY_P, "p", "P"},
        {KEY_LEFTBRACE, "bracketleft", "braceleft"},
        {KEY_RIGHTBRACE, "bracketright", "braceright"},
        {KEY_ENTER, "Return", nullptr},
        {KEY_LEFTCTRL, "Control_L", nullptr},
        {KEY_A, "a", "A"},
        {KEY_S, "s", "S"},
        {KEY_D, "d", "D"},
        {KEY_F, "f", "F"},
        {KEY_G, "g", "G"},
        {KEY_H, "h", "H"},
        {KEY_J, "j", "J"},
        {KEY_K, "k", "K"},
        {KEY_L, "l", "L"},
        {KEY_SEMICOLON, "semicolon", "colon"},
        {KEY_APOSTROPHE, "apostrophe", "quotedbl"},
        {KEY_GRAVE, "grave", "asciitilde"},
        {KEY_LEFTSHIFT, "Shift_L", nullptr},
        {KEY_BACKSLASH, "backslash", "bar"},
        {KEY_Z, "z", "Z"},
        {KEY_X, "x", "X"},
        {KEY_C, "c", "C"},
        {KEY_V, "v", "V"},
        {KEY_B, "b", "B"},
        {KEY_N, "n", "N"},
        {KEY_M, "m", "M"},
        {KEY_COMMA, "comma", "less"},
        {KEY_DOT, "period", "greater"},
        {KEY_SLASH, "slash", "question"},
        {KEY_RIGHTSHIFT, "Shift_R", nullptr},
        {KEY_LEFTALT, "Alt_L", nullptr},
        {KEY_SPACE, "space", nullptr},
        {KEY_CAPSLOCK, "Caps_Lock", nullptr},
        {KEY_F1, "F1", nullptr},
        {KEY_F2, "F2", nullptr},
        {KEY_F3, "F3", nullptr},
        {KEY_F4, "F4", nullptr},
        {KEY_F5, "F5", nullptr},
        {KEY_F6, "F6", nullptr},
        {KEY_F7, "F7", nullptr},
        {KEY_F8, "F8", nullptr},
        {KEY_F9, "F9", nullptr},
        {KEY_F10, "F10", nullptr},
        {KEY_F11, "F11", nullptr},
        {KEY_F12, "F12", nullptr},
        {KEY_HOME, "Home", nullptr},
        {KEY_UP, "Up", nullptr},
        {KEY_PAGEUP, "Prior", nullptr},
        {KEY_LEFT, "Left", nullptr},
        {KEY_RIGHT, "Right", nullptr},
        {KEY_END, "End", nullptr},
        {KEY_DOWN, "Down", nullptr},
        {KEY_PAGEDOWN, "Next", nullptr},
        {KEY_INSERT, "Insert", nullptr},
        {KEY_DELETE, "Delete", nullptr},
        {KEY_RIGHTCTRL, "Control_R", nullptr},
        {KEY_RIGHTALT, "Alt_R", nullptr},
        {KEY_LEFTMETA, "Super_L", nullptr},
        {KEY_RIGHTMETA, "Super_R", nullptr},
    };
    static constexpr size_t keyCount = sizeof(keyTable) / sizeof(keyTable[0]);

    // Clear mapping table
    memset(_linuxToCustom, 0, sizeof(_linuxToCustom));

    // Create temp file for keymap (wtype approach — more reliable than memfd)
    char tmpname[] = "/tmp/shards-xkb-XXXXXX";
    int fd = mkstemp(tmpname);
    if (fd < 0) {
      SPDLOG_LOGGER_ERROR(getWaylandInputLogger(), "mkstemp failed: {}", strerror(errno));
      return false;
    }
    unlink(tmpname);
    FILE *f = fdopen(fd, "w");
    if (!f) {
      close(fd);
      return false;
    }

    // Write XKB keymap — sequential keycodes starting from XKB 9 (evdev 1)
    fprintf(f, "xkb_keymap {\n");
    fprintf(f, "xkb_keycodes \"(unnamed)\" {\n"
               "minimum = 8;\n"
               "maximum = %zu;\n",
            keyCount + 8 + 1);
    for (size_t i = 0; i < keyCount; i++) {
      fprintf(f, "<K%zu> = %zu;\n", i + 1, i + 8 + 1);
      // Store mapping: linux keycode → custom evdev keycode (i+1)
      if (keyTable[i].linuxCode < MAX_KEYS) {
        _linuxToCustom[keyTable[i].linuxCode] = static_cast<uint32_t>(i + 1);
      }
    }
    fprintf(f, "};\n");

    fprintf(f, "xkb_types \"(unnamed)\" { include \"complete\" };\n");
    fprintf(f, "xkb_compatibility \"(unnamed)\" { include \"complete\" };\n");

    fprintf(f, "xkb_symbols \"(unnamed)\" {\n");
    for (size_t i = 0; i < keyCount; i++) {
      if (keyTable[i].shiftKeysym) {
        fprintf(f, "key <K%zu> {[%s, %s]};\n", i + 1, keyTable[i].keysym, keyTable[i].shiftKeysym);
      } else {
        fprintf(f, "key <K%zu> {[%s]};\n", i + 1, keyTable[i].keysym);
      }
    }
    // Modifier mappings
    fprintf(f, "modifier_map Shift {<K%u>, <K%u>};\n",
            _linuxToCustom[KEY_LEFTSHIFT], _linuxToCustom[KEY_RIGHTSHIFT]);
    fprintf(f, "modifier_map Control {<K%u>, <K%u>};\n",
            _linuxToCustom[KEY_LEFTCTRL], _linuxToCustom[KEY_RIGHTCTRL]);
    fprintf(f, "modifier_map Mod1 {<K%u>, <K%u>};\n",
            _linuxToCustom[KEY_LEFTALT], _linuxToCustom[KEY_RIGHTALT]);
    fprintf(f, "modifier_map Mod4 {<K%u>, <K%u>};\n",
            _linuxToCustom[KEY_LEFTMETA], _linuxToCustom[KEY_RIGHTMETA]);
    fprintf(f, "};\n");
    fprintf(f, "};\n");
    fputc('\0', f);
    fflush(f);
    size_t keymapSize = ftell(f);

    SPDLOG_LOGGER_INFO(getWaylandInputLogger(), "Generated custom XKB keymap: {} bytes, {} keys", keymapSize, keyCount);

    zwp_virtual_keyboard_v1_keymap(_keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fileno(f), keymapSize);
    wl_display_roundtrip(_display);
    fclose(f);

    return true;
  }

  static void registryGlobal(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t ver) {
    auto *self = static_cast<WaylandVirtualInput *>(data);
    if (!strcmp(iface, wl_seat_interface.name)) {
      self->_seat =
          static_cast<struct wl_seat *>(wl_registry_bind(reg, name, &wl_seat_interface, ver <= 7 ? ver : 7));
    } else if (!strcmp(iface, zwp_virtual_keyboard_manager_v1_interface.name)) {
      self->_kbManager = static_cast<struct zwp_virtual_keyboard_manager_v1 *>(
          wl_registry_bind(reg, name, &zwp_virtual_keyboard_manager_v1_interface, 1));
    } else if (!strcmp(iface, zwlr_virtual_pointer_manager_v1_interface.name)) {
      self->_ptrManager = static_cast<struct zwlr_virtual_pointer_manager_v1 *>(
          wl_registry_bind(reg, name, &zwlr_virtual_pointer_manager_v1_interface, 1));
    }
  }

  static void registryGlobalRemove(void *, struct wl_registry *, uint32_t) {}

  static constexpr struct wl_registry_listener registryListener = {
      .global = registryGlobal,
      .global_remove = registryGlobalRemove,
  };

  struct wl_display *_display = nullptr;
  struct wl_registry *_registry = nullptr;
  struct wl_seat *_seat = nullptr;
  struct zwp_virtual_keyboard_manager_v1 *_kbManager = nullptr;
  struct zwlr_virtual_pointer_manager_v1 *_ptrManager = nullptr;
  struct zwp_virtual_keyboard_v1 *_keyboard = nullptr;
  struct zwlr_virtual_pointer_v1 *_pointer = nullptr;
  bool _available = false;
  bool _hasKeyboard = false;
  bool _hasPointer = false;
  uint32_t _modState = 0;
  uint32_t _linuxToCustom[MAX_KEYS] = {};
};

} // namespace Desktop

#endif // SH_DESKTOP_WAYLAND_INPUT_LINUX
