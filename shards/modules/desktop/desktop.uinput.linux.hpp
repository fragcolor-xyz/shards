/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SH_DESKTOP_UINPUT_LINUX
#define SH_DESKTOP_UINPUT_LINUX

#include <shards/log/log.hpp>
#include <linux/uinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <thread>
#include <chrono>

namespace Desktop {

static inline shards::logging::Logger getUInputLogger() {
  static auto logger = shards::logging::getOrCreate("Desktop.UInput");
  return logger;
}

// Virtual input device via /dev/uinput for compositor-independent input injection.
// Creates two devices: one for keyboard, one for mouse (relative + absolute + buttons + scroll).
class UInputDevice {
public:
  static constexpr int ABS_RANGE = 32767;

  UInputDevice() = default;
  ~UInputDevice() { shutdown(); }

  UInputDevice(const UInputDevice &) = delete;
  UInputDevice &operator=(const UInputDevice &) = delete;

  bool init() {
    _kbFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (_kbFd < 0) {
      SPDLOG_LOGGER_ERROR(getUInputLogger(),
                          "Failed to open /dev/uinput for keyboard: {} ({}). "
                          "Ensure your user is in the 'input' group and a udev rule grants write access:\n"
                          "  sudo usermod -aG input $USER\n"
                          "  echo 'KERNEL==\"uinput\", GROUP=\"input\", MODE=\"0660\"' | sudo tee /etc/udev/rules.d/99-uinput.rules\n"
                          "  sudo udevadm control --reload-rules && sudo udevadm trigger",
                          strerror(errno), errno);
      return false;
    }

    _mouseFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (_mouseFd < 0) {
      SPDLOG_LOGGER_ERROR(getUInputLogger(), "Failed to open /dev/uinput for mouse: {} ({})", strerror(errno), errno);
      close(_kbFd);
      _kbFd = -1;
      return false;
    }

    if (!setupKeyboard() || !setupMouse()) {
      shutdown();
      return false;
    }

    // Wait for kernel to register the devices
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    _available = true;
    SPDLOG_LOGGER_INFO(getUInputLogger(), "UInput devices created (keyboard fd={}, mouse fd={})", _kbFd, _mouseFd);
    return true;
  }

  void shutdown() {
    _available = false;
    if (_kbFd >= 0) {
      ioctl(_kbFd, UI_DEV_DESTROY);
      close(_kbFd);
      _kbFd = -1;
    }
    if (_mouseFd >= 0) {
      ioctl(_mouseFd, UI_DEV_DESTROY);
      close(_mouseFd);
      _mouseFd = -1;
    }
  }

  bool isAvailable() const { return _available; }

  void keyboardKey(uint32_t keycode, bool pressed) {
    if (!_available)
      return;
    emit(_kbFd, EV_KEY, keycode, pressed ? 1 : 0);
    emit(_kbFd, EV_SYN, SYN_REPORT, 0);
  }

  void pointerMotionRelative(int dx, int dy) {
    if (!_available)
      return;
    emit(_mouseFd, EV_REL, REL_X, dx);
    emit(_mouseFd, EV_REL, REL_Y, dy);
    emit(_mouseFd, EV_SYN, SYN_REPORT, 0);
  }

  void pointerMotionAbsolute(int x, int y, int screenW, int screenH) {
    if (!_available || screenW <= 0 || screenH <= 0)
      return;
    int absX = (int)((int64_t)x * ABS_RANGE / screenW);
    int absY = (int)((int64_t)y * ABS_RANGE / screenH);
    emit(_mouseFd, EV_ABS, ABS_X, absX);
    emit(_mouseFd, EV_ABS, ABS_Y, absY);
    emit(_mouseFd, EV_SYN, SYN_REPORT, 0);
  }

  void pointerButton(uint32_t button, bool pressed) {
    if (!_available)
      return;
    emit(_mouseFd, EV_KEY, button, pressed ? 1 : 0);
    emit(_mouseFd, EV_SYN, SYN_REPORT, 0);
  }

  void scrollVertical(int amount) {
    if (!_available)
      return;
    emit(_mouseFd, EV_REL, REL_WHEEL, amount);
    emit(_mouseFd, EV_SYN, SYN_REPORT, 0);
  }

  void scrollHorizontal(int amount) {
    if (!_available)
      return;
    emit(_mouseFd, EV_REL, REL_HWHEEL, amount);
    emit(_mouseFd, EV_SYN, SYN_REPORT, 0);
  }

private:
  bool setupKeyboard() {
    if (ioctl(_kbFd, UI_SET_EVBIT, EV_KEY) < 0)
      return false;

    // Enable all standard key codes
    for (int i = 0; i < KEY_CNT; i++) {
      ioctl(_kbFd, UI_SET_KEYBIT, i);
    }

    struct uinput_setup setup {};
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "shards-virtual-keyboard");
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x0001;
    setup.id.version = 1;

    if (ioctl(_kbFd, UI_DEV_SETUP, &setup) < 0) {
      SPDLOG_LOGGER_ERROR(getUInputLogger(), "UI_DEV_SETUP failed for keyboard: {}", strerror(errno));
      return false;
    }
    if (ioctl(_kbFd, UI_DEV_CREATE) < 0) {
      SPDLOG_LOGGER_ERROR(getUInputLogger(), "UI_DEV_CREATE failed for keyboard: {}", strerror(errno));
      return false;
    }
    return true;
  }

  bool setupMouse() {
    // Buttons
    if (ioctl(_mouseFd, UI_SET_EVBIT, EV_KEY) < 0)
      return false;
    ioctl(_mouseFd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(_mouseFd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(_mouseFd, UI_SET_KEYBIT, BTN_MIDDLE);
    ioctl(_mouseFd, UI_SET_KEYBIT, BTN_SIDE);
    ioctl(_mouseFd, UI_SET_KEYBIT, BTN_EXTRA);

    // Relative motion + scroll
    if (ioctl(_mouseFd, UI_SET_EVBIT, EV_REL) < 0)
      return false;
    ioctl(_mouseFd, UI_SET_RELBIT, REL_X);
    ioctl(_mouseFd, UI_SET_RELBIT, REL_Y);
    ioctl(_mouseFd, UI_SET_RELBIT, REL_WHEEL);
    ioctl(_mouseFd, UI_SET_RELBIT, REL_HWHEEL);

    // Absolute position
    if (ioctl(_mouseFd, UI_SET_EVBIT, EV_ABS) < 0)
      return false;

    struct uinput_abs_setup absSetupX {};
    absSetupX.code = ABS_X;
    absSetupX.absinfo.minimum = 0;
    absSetupX.absinfo.maximum = ABS_RANGE;
    absSetupX.absinfo.resolution = 1;
    if (ioctl(_mouseFd, UI_ABS_SETUP, &absSetupX) < 0) {
      SPDLOG_LOGGER_ERROR(getUInputLogger(), "UI_ABS_SETUP ABS_X failed: {}", strerror(errno));
      return false;
    }

    struct uinput_abs_setup absSetupY {};
    absSetupY.code = ABS_Y;
    absSetupY.absinfo.minimum = 0;
    absSetupY.absinfo.maximum = ABS_RANGE;
    absSetupY.absinfo.resolution = 1;
    if (ioctl(_mouseFd, UI_ABS_SETUP, &absSetupY) < 0) {
      SPDLOG_LOGGER_ERROR(getUInputLogger(), "UI_ABS_SETUP ABS_Y failed: {}", strerror(errno));
      return false;
    }

    struct uinput_setup setup {};
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "shards-virtual-mouse");
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x0002;
    setup.id.version = 1;

    if (ioctl(_mouseFd, UI_DEV_SETUP, &setup) < 0) {
      SPDLOG_LOGGER_ERROR(getUInputLogger(), "UI_DEV_SETUP failed for mouse: {}", strerror(errno));
      return false;
    }
    if (ioctl(_mouseFd, UI_DEV_CREATE) < 0) {
      SPDLOG_LOGGER_ERROR(getUInputLogger(), "UI_DEV_CREATE failed for mouse: {}", strerror(errno));
      return false;
    }
    return true;
  }

  void emit(int fd, uint16_t type, uint16_t code, int32_t value) {
    struct input_event ev {};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(fd, &ev, sizeof(ev)) < 0) {
      SPDLOG_LOGGER_TRACE(getUInputLogger(), "write failed: {}", strerror(errno));
    }
  }

  int _kbFd = -1;
  int _mouseFd = -1;
  bool _available = false;
};

} // namespace Desktop

#endif // SH_DESKTOP_UINPUT_LINUX
