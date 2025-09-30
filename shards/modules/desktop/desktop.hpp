/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_EXTRA_DESKTOP
#define SH_EXTRA_DESKTOP

#include <shards/core/shared.hpp>
#include <shards/core/runtime.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <cstdlib>

namespace Desktop {
constexpr uint32_t windowCC = 'hwnd';

struct Globals {
  static inline shards::Type windowType{{SHType::Object, {.object = {.vendorId = shards::CoreCC, .typeId = windowCC}}}};
  static inline shards::Type windowVarType = shards::Type::VariableOf(windowType);
  static inline shards::Types windowVarOrNone{{windowVarType, shards::CoreInfo::NoneType}};
};

template <typename T> class WindowBase {
public:
  PARAM_PARAMVAR(_winName, "Title", "The title of the window to look for.", {shards::CoreInfo::StringType, shards::CoreInfo::StringVarType});
  PARAM_PARAMVAR(_winClass, "Class", "An optional and platform dependent window class.", {shards::CoreInfo::StringType, shards::CoreInfo::StringVarType, shards::CoreInfo::NoneType});
  PARAM_IMPL(PARAM_IMPL_FOR(_winName), PARAM_IMPL_FOR(_winClass));

  void cleanup(SHContext *context) {
    // reset to default
    // force finding it again next run
    _window = WindowDefault();
    PARAM_CLEANUP(context);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
  }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }

protected:
  static T WindowDefault();
  T _window;
};

struct ActiveBase {
  static SHTypesInfo inputTypes() { return Globals::windowType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::BoolType; }
};

struct PIDBase {
  static SHTypesInfo inputTypes() { return Globals::windowType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::IntType; }
};

struct WinOpBase {
  static SHTypesInfo inputTypes() { return Globals::windowType; }
  static SHTypesInfo outputTypes() { return Globals::windowType; }
};

struct SizeBase {
  static SHTypesInfo inputTypes() { return Globals::windowType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Int2Type; }
};

struct ResizeWindowBase : public WinOpBase {
  PARAM_VAR(_width, "Width", "The desired width.", {shards::CoreInfo::IntType});
  PARAM_VAR(_height, "Height", "The desired height.", {shards::CoreInfo::IntType});
  PARAM_IMPL(PARAM_IMPL_FOR(_width), PARAM_IMPL_FOR(_height));
};

struct MoveWindowBase : public WinOpBase {
  PARAM_VAR(_x, "X", "The desired horizontal coordinates.", {shards::CoreInfo::IntType});
  PARAM_VAR(_y, "Y", "The desired vertical coordinates.", {shards::CoreInfo::IntType});
  PARAM_IMPL(PARAM_IMPL_FOR(_x), PARAM_IMPL_FOR(_y));
};

struct SetTitleBase : public WinOpBase {
  PARAM_VAR(_title, "Title", "The title to set for the window.", {shards::CoreInfo::StringType});
  PARAM_IMPL(PARAM_IMPL_FOR(_title));
};

struct WaitKeyEventBase {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Int2Type; }

  static SHOptionalString help() {
    return SHCCSTR("### Pauses the wire and waits for keyboard events.\n#### The output "
                   "of this shard will be a Int2.\n * The first integer will be 0 for Key "
                   "down/push events and 1 for Key up/release events.\n * The second "
                   "integer will the scancode of the key.\n");
  }
};

struct SendKeyEventBase {
  PARAM_PARAMVAR(_window, "Window", "None or a window variable if we wish to send the event only to a specific target window.", {Globals::windowType, Globals::windowVarType, shards::CoreInfo::NoneType});
  PARAM_IMPL(PARAM_IMPL_FOR(_window));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Int2Type; }

  static SHOptionalString help() {
    return SHCCSTR("### Sends the input key event.\n#### The input of this "
                   "shard will be a Int2.\n * The first integer will be 0 for "
                   "Key down/push events and 1 for Key up/release events.\n * "
                   "The second integer will the scancode of the key.\n");
  }
};

struct MousePosBase {
  PARAM_PARAMVAR(_window, "Window", "None or a window variable we wish to use as relative origin.", {Globals::windowType, Globals::windowVarType, shards::CoreInfo::NoneType});
  PARAM_IMPL(PARAM_IMPL_FOR(_window));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Int2Type; }
};

struct LastInputBase {
  // outputs the seconds since the last input happened
  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::FloatType; }
};
}; // namespace Desktop

#endif // SH_EXTRA_DESKTOP
