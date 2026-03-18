/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include "shards/core/module.hpp"
#include "desktop.hpp"
#include "desktop.portal.linux.hpp"
#include "desktop.capture.linux.hpp"
#include "desktop.uinput.linux.hpp"

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/core/runtime.hpp>
#include <shards/common_types.hpp>

#include <linux/input-event-codes.h>

using namespace shards;

namespace Desktop {

// Session tag to distinguish portal vs direct sessions stored as the same Object type.
// Both PortalSession* and DirectSession* are stored as SHType::Object with windowCC,
// so we use a tag byte at the start of each struct to tell them apart at runtime.
enum class SessionKind : uint8_t { Portal = 1, Direct = 2 };

// Tagged base — first byte identifies the session type
struct SessionTag {
  SessionKind kind;
};

// Helper to extract a pointer from object SHVar and identify the session kind
static void *asSessionPtr(const SHVar &var, SessionKind &outKind) {
  if (var.valueType == SHType::Object && var.payload.objectVendorId == CoreCC && var.payload.objectTypeId == windowCC) {
    auto *tag = reinterpret_cast<SessionTag *>(var.payload.objectValue);
    outKind = tag->kind;
    return var.payload.objectValue;
  }
  outKind = SessionKind::Portal;
  return nullptr;
}

// Global capture instances keyed by portal session
static std::unordered_map<PortalSession *, std::unique_ptr<PipeWireCapture>> g_captures;
static std::mutex g_capturesMutex;

static PipeWireCapture *getOrCreateCapture(PortalSession *session) {
  std::lock_guard<std::mutex> lock(g_capturesMutex);
  auto it = g_captures.find(session);
  if (it != g_captures.end())
    return it->second.get();

  if (!session->isActive() || session->pipewireFd() < 0)
    return nullptr;

  auto capture = std::make_unique<PipeWireCapture>();
  if (!capture->init(session->pipewireFd(), session->pipewireNode())) {
    return nullptr;
  }
  auto *ptr = capture.get();
  g_captures[session] = std::move(capture);
  return ptr;
}

static void removeCapture(PortalSession *session) {
  std::lock_guard<std::mutex> lock(g_capturesMutex);
  g_captures.erase(session);
}

// Global UInputDevice singleton for input injection
static std::unique_ptr<UInputDevice> g_uinput;
static std::mutex g_uinputMutex;

static UInputDevice *getOrCreateUInput() {
  std::lock_guard<std::mutex> lock(g_uinputMutex);
  if (g_uinput && g_uinput->isAvailable())
    return g_uinput.get();

  if (!g_uinput) {
    g_uinput = std::make_unique<UInputDevice>();
    if (!g_uinput->init()) {
      g_uinput.reset();
      return nullptr;
    }
  }
  return g_uinput.get();
}

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.StartSession
// Opens a portal ScreenCast session (with RemoteDesktop upgrade if available).
// Returns a session object that can be used for capture and input injection.
// ─────────────────────────────────────────────────────────────────────────────

struct StartSession {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Globals::windowType; }

  static SHOptionalString help() {
    return SHCCSTR("Opens an xdg-desktop-portal session for screen capture. "
                   "Tries RemoteDesktop first, falls back to ScreenCast on compositors that only support it. "
                   "The user will be prompted to select a screen or window to share. "
                   "Returns a session object for use with capture and input injection shards.");
  }

  PortalSession *_session = nullptr;
  SHVar _output{};

  void cleanup(SHContext *context) {
    if (_session) {
      removeCapture(_session);
      delete _session;
      _session = nullptr;
    }
    _output = SHVar{};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_session) {
      _session = new PortalSession();
      _session->start();
    }

    while (_session->state() == PortalSession::State::Pending) {
      _session->poll(); // pump GLib main context for D-Bus callbacks
      SH_SUSPEND(context, 0.1);
    }

    if (_session->state() == PortalSession::State::Failed) {
      throw ActivationError("Portal session failed - user may have denied access");
    }

    // Also initialize UInput for input injection
    getOrCreateUInput();

    _output = Var::Object(_session, CoreCC, windowCC);
    return _output;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.StartDirectCapture
// Connects directly to PipeWire by node ID — no portal, no consent dialog.
// For headless/automated scenarios (CI, testing, remote desktop servers).
// Input: Int (PipeWire node ID)
// ─────────────────────────────────────────────────────────────────────────────

// Sentinel object to represent a direct capture session (no portal).
// Tag byte must be first member (matches SessionTag layout for reinterpret_cast).
struct DirectSession {
  uint8_t _sessionKind = static_cast<uint8_t>(SessionKind::Direct);
  uint32_t nodeId = 0;
};

static std::unordered_map<DirectSession *, std::unique_ptr<PipeWireCapture>> g_directCaptures;
static std::mutex g_directCapturesMutex;

static PipeWireCapture *getDirectCapture(DirectSession *ds) {
  std::lock_guard<std::mutex> lock(g_directCapturesMutex);
  auto it = g_directCaptures.find(ds);
  return it != g_directCaptures.end() ? it->second.get() : nullptr;
}

static void removeDirectCapture(DirectSession *ds) {
  std::lock_guard<std::mutex> lock(g_directCapturesMutex);
  g_directCaptures.erase(ds);
}

struct StartDirectCapture {
  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHTypesInfo outputTypes() { return Globals::windowType; }

  static SHOptionalString help() {
    return SHCCSTR("Starts a direct PipeWire capture by node ID — no portal, no consent dialog. "
                   "For headless/automated scenarios. Input is the PipeWire node ID (integer). "
                   "Find node IDs with: pw-cli list-objects | grep -A5 'node.name'");
  }

  DirectSession *_session = nullptr;
  SHVar _output{};

  void cleanup(SHContext *context) {
    if (_session) {
      removeDirectCapture(_session);
      delete _session;
      _session = nullptr;
    }
    _output = SHVar{};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (_session) {
      _output = Var::Object(_session, CoreCC, windowCC);
      return _output;
    }

    uint32_t nodeId = static_cast<uint32_t>(input.payload.intValue);

    _session = new DirectSession();
    _session->nodeId = nodeId;
    auto capture = std::make_unique<PipeWireCapture>();
    if (!capture->initDirect(nodeId)) {
      delete _session;
      _session = nullptr;
      throw ActivationError("Failed to connect to PipeWire node directly");
    }

    {
      std::lock_guard<std::mutex> lock(g_directCapturesMutex);
      g_directCaptures[_session] = std::move(capture);
    }

    // Also initialize UInput for input injection
    getOrCreateUInput();

    _output = Var::Object(_session, CoreCC, windowCC);
    return _output;
  }
};

// Unified capture lookup — works for both portal and direct sessions
static PipeWireCapture *getCaptureForSession(const SHVar &var) {
  SessionKind kind;
  auto *ptr = asSessionPtr(var, kind);
  if (!ptr)
    return nullptr;

  if (kind == SessionKind::Portal) {
    return getOrCreateCapture(reinterpret_cast<PortalSession *>(ptr));
  } else {
    return getDirectCapture(reinterpret_cast<DirectSession *>(ptr));
  }
}

// Unified size lookup
static bool getCaptureSizeForSession(const SHVar &var, int &w, int &h) {
  auto *capture = getCaptureForSession(var);
  if (capture && capture->hasFrame()) {
    w = capture->width();
    h = capture->height();
    return true;
  }
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.CaptureFrame
// Updates the capture buffer from the PipeWire stream.
// ─────────────────────────────────────────────────────────────────────────────

struct CaptureFrame {
  static SHTypesInfo inputTypes() { return Globals::windowType; }
  static SHTypesInfo outputTypes() { return Globals::windowType; }

  static SHOptionalString help() {
    return SHCCSTR("Updates the screen capture from the PipeWire stream. "
                   "Call this before using Pixel or Pixels shards.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *capture = getCaptureForSession(input);
    if (capture) {
      capture->update();
    }
    return input;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.Pixel
// Reads a single pixel from the captured frame at coordinates (x, y).
// ─────────────────────────────────────────────────────────────────────────────

struct Pixel {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::ColorType; }

  static SHOptionalString help() {
    return SHCCSTR("Reads a single pixel color from the captured screen at the given Int2 coordinates.");
  }

  PARAM_PARAMVAR(_session, "Session", "The capture session to read from.", {Globals::windowVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_session));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *capture = getCaptureForSession(_session.get());
    if (!capture || !capture->hasFrame())
      throw ActivationError("No capture frame available");

    int x = input.payload.int2Value[0];
    int y = input.payload.int2Value[1];
    int w = capture->width();
    int h = capture->height();

    if (x < 0 || x >= w || y < 0 || y >= h)
      throw ActivationError("Pixel coordinates out of bounds");

    const uint8_t *img = capture->image();
    int offset = (y * w + x) * 4;
    // BGRA format
    SHVar result{};
    result.valueType = SHType::Color;
    result.payload.colorValue.b = img[offset + 0];
    result.payload.colorValue.g = img[offset + 1];
    result.payload.colorValue.r = img[offset + 2];
    result.payload.colorValue.a = img[offset + 3];
    return result;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.Pixels
// Reads a region of pixels from the captured frame.
// Input: Int4 [left, top, right, bottom]
// Output: Image
// ─────────────────────────────────────────────────────────────────────────────

struct Pixels {
  static SHTypesInfo inputTypes() { return CoreInfo::Int4Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static SHOptionalString help() {
    return SHCCSTR("Captures a region of pixels from the screen. "
                   "Input is Int4 [left, top, right, bottom]. Output is an Image.");
  }

  PARAM_PARAMVAR(_session, "Session", "The capture session to read from.", {Globals::windowVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_session));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _output = Var::Empty;
  }

  OwnedVar _output{};

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *capture = getCaptureForSession(_session.get());
    if (!capture || !capture->hasFrame())
      throw ActivationError("No capture frame available");

    int left = input.payload.int4Value[0];
    int top = input.payload.int4Value[1];
    int right = input.payload.int4Value[2];
    int bottom = input.payload.int4Value[3];

    int srcW = capture->width();
    int srcH = capture->height();

    // Clamp to valid range
    left = std::max(0, std::min(left, srcW));
    top = std::max(0, std::min(top, srcH));
    right = std::max(left, std::min(right, srcW));
    bottom = std::max(top, std::min(bottom, srcH));

    int w = right - left;
    int h = bottom - top;

    _output = makeImage(w * h * 4);
    _output.payload.imageValue->width = static_cast<uint16_t>(w);
    _output.payload.imageValue->height = static_cast<uint16_t>(h);
    _output.payload.imageValue->channels = 4;
    _output.payload.imageValue->flags = 0;
    SHImage &outImage = *_output.payload.imageValue;

    const uint8_t *img = capture->image();
    for (int y = 0; y < h; y++) {
      const uint8_t *srcRow = img + ((top + y) * srcW + left) * 4;
      uint8_t *dstRow = outImage.data + y * w * 4;
      std::memcpy(dstRow, srcRow, w * 4);
    }

    return _output;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.SendKeyEvent
// Sends a keyboard event via uinput.
// Input: Int2 [state (0=down, 1=up), linux keycode]
// ─────────────────────────────────────────────────────────────────────────────

struct SendKeyEvent {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  static SHOptionalString help() {
    return SHCCSTR("Sends a keyboard event via uinput virtual device. "
                   "Input is Int2 [state (0=down, 1=up), linux keycode].");
  }

  PARAM_PARAMVAR(_session, "Session", "The desktop session (used for API compatibility).", {Globals::windowVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_session));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *uinput = getOrCreateUInput();
    if (!uinput)
      throw ActivationError("UInput not available - check /dev/uinput permissions");

    int state = input.payload.int2Value[0];
    int keycode = input.payload.int2Value[1];
    bool pressed = (state == 0); // 0 = down/pressed

    uinput->keyboardKey(static_cast<uint32_t>(keycode), pressed);
    return input;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.SetMousePos
// Sets the absolute mouse position via uinput.
// Input: Int2 [x, y]
// ─────────────────────────────────────────────────────────────────────────────

struct SetMousePos {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  static SHOptionalString help() {
    return SHCCSTR("Sets the mouse position to absolute coordinates via uinput. "
                   "Input is Int2 [x, y]. Coordinates are scaled using the capture dimensions.");
  }

  PARAM_PARAMVAR(_session, "Session", "The desktop session (needed for capture dimensions).", {Globals::windowVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_session));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *uinput = getOrCreateUInput();
    if (!uinput)
      throw ActivationError("UInput not available - check /dev/uinput permissions");

    int x = input.payload.int2Value[0];
    int y = input.payload.int2Value[1];

    // Get screen dimensions from capture for coordinate scaling
    int screenW = 1920, screenH = 1080; // defaults
    getCaptureSizeForSession(_session.get(), screenW, screenH);

    uinput->pointerMotionAbsolute(x, y, screenW, screenH);
    return input;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.SetMouseRelativePos
// Moves the mouse by a relative delta via uinput.
// Input: Int2 [dx, dy]
// ─────────────────────────────────────────────────────────────────────────────

struct SetMouseRelativePos {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  static SHOptionalString help() {
    return SHCCSTR("Moves the mouse by a relative delta via uinput. "
                   "Input is Int2 [dx, dy].");
  }

  PARAM_PARAMVAR(_session, "Session", "The desktop session (used for API compatibility).", {Globals::windowVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_session));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *uinput = getOrCreateUInput();
    if (!uinput)
      throw ActivationError("UInput not available - check /dev/uinput permissions");

    int dx = input.payload.int2Value[0];
    int dy = input.payload.int2Value[1];
    uinput->pointerMotionRelative(dx, dy);
    return input;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Mouse click helpers
// ─────────────────────────────────────────────────────────────────────────────

struct ClickBase {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_PARAMVAR(_session, "Session", "The desktop session (needed for capture dimensions).", {Globals::windowVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_session));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

protected:
  SHVar doClick(SHContext *context, const SHVar &input, uint32_t button) {
    auto *uinput = getOrCreateUInput();
    if (!uinput)
      throw ActivationError("UInput not available - check /dev/uinput permissions");

    int x = input.payload.int2Value[0];
    int y = input.payload.int2Value[1];

    // Get screen dimensions from capture for coordinate scaling
    int screenW = 1920, screenH = 1080; // defaults
    getCaptureSizeForSession(_session.get(), screenW, screenH);

    // Move to position first, then click
    uinput->pointerMotionAbsolute(x, y, screenW, screenH);
    uinput->pointerButton(button, true);
    uinput->pointerButton(button, false);

    return input;
  }
};

struct LeftClick : public ClickBase {
  static SHOptionalString help() { return SHCCSTR("Performs a left click at the given Int2 position."); }
  SHVar activate(SHContext *context, const SHVar &input) { return doClick(context, input, BTN_LEFT); }
};

struct RightClick : public ClickBase {
  static SHOptionalString help() { return SHCCSTR("Performs a right click at the given Int2 position."); }
  SHVar activate(SHContext *context, const SHVar &input) { return doClick(context, input, BTN_RIGHT); }
};

struct MiddleClick : public ClickBase {
  static SHOptionalString help() { return SHCCSTR("Performs a middle click at the given Int2 position."); }
  SHVar activate(SHContext *context, const SHVar &input) { return doClick(context, input, BTN_MIDDLE); }
};

// ─────────────────────────────────────────────────────────────────────────────
// Desktop.ScrollVertical / Desktop.ScrollHorizontal
// ─────────────────────────────────────────────────────────────────────────────

struct ScrollVertical {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  static SHOptionalString help() { return SHCCSTR("Scrolls vertically by the given float amount via uinput."); }

  PARAM_PARAMVAR(_session, "Session", "The desktop session (used for API compatibility).", {Globals::windowVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_session));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *uinput = getOrCreateUInput();
    if (!uinput)
      throw ActivationError("UInput not available - check /dev/uinput permissions");

    int amount = static_cast<int>(input.payload.floatValue);
    uinput->scrollVertical(amount);
    return input;
  }
};

struct ScrollHorizontal {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  static SHOptionalString help() { return SHCCSTR("Scrolls horizontally by the given float amount via uinput."); }

  PARAM_PARAMVAR(_session, "Session", "The desktop session (used for API compatibility).", {Globals::windowVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_session));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *uinput = getOrCreateUInput();
    if (!uinput)
      throw ActivationError("UInput not available - check /dev/uinput permissions");

    int amount = static_cast<int>(input.payload.floatValue);
    uinput->scrollHorizontal(amount);
    return input;
  }
};

} // namespace Desktop

// ─────────────────────────────────────────────────────────────────────────────
// Registration
// ─────────────────────────────────────────────────────────────────────────────

SHARDS_REGISTER_FN(desktop) {
  using namespace Desktop;

  // Session management
  REGISTER_SHARD("Desktop.StartSession", StartSession);
  REGISTER_SHARD("Desktop.StartDirectCapture", StartDirectCapture);

  // Screen capture
  REGISTER_SHARD("Desktop.CaptureFrame", CaptureFrame);
  REGISTER_SHARD("Desktop.Pixel", Pixel);
  REGISTER_SHARD("Desktop.Pixels", Pixels);

  // Input injection (via uinput)
  REGISTER_SHARD("Desktop.SendKeyEvent", SendKeyEvent);
  REGISTER_SHARD("Desktop.SetMousePos", SetMousePos);
  REGISTER_SHARD("Desktop.SetMouseRelativePos", SetMouseRelativePos);
  REGISTER_SHARD("Desktop.LeftClick", LeftClick);
  REGISTER_SHARD("Desktop.RightClick", RightClick);
  REGISTER_SHARD("Desktop.MiddleClick", MiddleClick);
  REGISTER_SHARD("Desktop.ScrollVertical", ScrollVertical);
  REGISTER_SHARD("Desktop.ScrollHorizontal", ScrollHorizontal);
}
