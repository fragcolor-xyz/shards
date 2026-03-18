/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SH_DESKTOP_WAYLAND_INPUT_LINUX
#define SH_DESKTOP_WAYLAND_INPUT_LINUX

#include <shards/log/log.hpp>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

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
class WaylandVirtualInput {
public:
  WaylandVirtualInput() = default;
  ~WaylandVirtualInput() { shutdown(); }

  WaylandVirtualInput(const WaylandVirtualInput &) = delete;
  WaylandVirtualInput &operator=(const WaylandVirtualInput &) = delete;

  bool init() {
    const char *waylandDisplay = getenv("WAYLAND_DISPLAY");
    SPDLOG_LOGGER_INFO(getWaylandInputLogger(), "Connecting to Wayland display: {}", waylandDisplay ? waylandDisplay : "(default)");
    _display = wl_display_connect(nullptr);
    if (!_display) {
      SPDLOG_LOGGER_DEBUG(getWaylandInputLogger(), "Failed to connect to Wayland display");
      return false;
    }

    _registry = wl_display_get_registry(_display);
    wl_registry_add_listener(_registry, &registryListener, this);
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

  void keyboardKey(uint32_t keycode, bool pressed) {
    if (!_available || !_hasKeyboard)
      return;
    SPDLOG_LOGGER_TRACE(getWaylandInputLogger(), "key {} {}", keycode, pressed ? "down" : "up");
    zwp_virtual_keyboard_v1_key(_keyboard, nowMs(), keycode,
                                pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
    int ret = wl_display_flush(_display);
    if (ret < 0) {
      SPDLOG_LOGGER_ERROR(getWaylandInputLogger(), "wl_display_flush failed: {}", strerror(errno));
    }
  }

  void pointerMotionRelative(int dx, int dy) {
    if (!_available || !_hasPointer)
      return;
    zwlr_virtual_pointer_v1_motion(_pointer, nowMs(), wl_fixed_from_int(dx), wl_fixed_from_int(dy));
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_flush(_display);
  }

  void pointerMotionAbsolute(int x, int y, int screenW, int screenH) {
    if (!_available || !_hasPointer || screenW <= 0 || screenH <= 0)
      return;
    zwlr_virtual_pointer_v1_motion_absolute(_pointer, nowMs(), (uint32_t)x, (uint32_t)y, (uint32_t)screenW, (uint32_t)screenH);
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_flush(_display);
  }

  void pointerButton(uint32_t button, bool pressed) {
    if (!_available || !_hasPointer)
      return;
    zwlr_virtual_pointer_v1_button(_pointer, nowMs(), button,
                                   pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_flush(_display);
  }

  void scrollVertical(int amount) {
    if (!_available || !_hasPointer)
      return;
    zwlr_virtual_pointer_v1_axis_discrete(_pointer, nowMs(), WL_POINTER_AXIS_VERTICAL_SCROLL,
                                          wl_fixed_from_int(amount * 15), amount);
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_flush(_display);
  }

  void scrollHorizontal(int amount) {
    if (!_available || !_hasPointer)
      return;
    zwlr_virtual_pointer_v1_axis_discrete(_pointer, nowMs(), WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                                          wl_fixed_from_int(amount * 15), amount);
    zwlr_virtual_pointer_v1_frame(_pointer);
    wl_display_flush(_display);
  }

private:
  static uint32_t nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
  }

  bool setupKeymap() {
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!ctx)
      return false;

    struct xkb_rule_names names{};
    names.rules = "evdev";
    names.model = "pc105";
    names.layout = "us";

    struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
      xkb_context_unref(ctx);
      return false;
    }

    char *keymapStr = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    if (!keymapStr) {
      xkb_keymap_unref(keymap);
      xkb_context_unref(ctx);
      return false;
    }

    size_t keymapSize = strlen(keymapStr) + 1;
    int fd = memfd_create("xkb-keymap", MFD_CLOEXEC);
    if (fd < 0) {
      free(keymapStr);
      xkb_keymap_unref(keymap);
      xkb_context_unref(ctx);
      return false;
    }

    if (ftruncate(fd, keymapSize) < 0 || (size_t)write(fd, keymapStr, keymapSize) != keymapSize) {
      close(fd);
      free(keymapStr);
      xkb_keymap_unref(keymap);
      xkb_context_unref(ctx);
      return false;
    }

    zwp_virtual_keyboard_v1_keymap(_keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, keymapSize);
    wl_display_flush(_display);

    close(fd);
    free(keymapStr);
    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);
    return true;
  }

  static void registryGlobal(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t ver) {
    auto *self = static_cast<WaylandVirtualInput *>(data);
    if (!strcmp(iface, wl_seat_interface.name)) {
      self->_seat = static_cast<struct wl_seat *>(wl_registry_bind(reg, name, &wl_seat_interface, 1));
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
};

} // namespace Desktop

#endif // SH_DESKTOP_WAYLAND_INPUT_LINUX
