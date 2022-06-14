/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_EXTRA_DESKTOP
#define SH_EXTRA_DESKTOP

#include "runtime.hpp"
#include "shards/shared.hpp"
#include <cstdlib>

namespace Desktop {
constexpr uint32_t windowCC = 'hwnd';

struct Globals {
  static inline shards::Type windowType{{SHType::Object, {.object = {.vendorId = shards::CoreCC, .typeId = windowCC}}}};
  static inline shards::Types windowVarOrNone{{windowType, shards::CoreInfo::NoneType}};
};

template <typename T> class WindowBase {
public:
  void cleanup() {
    // reset to default
    // force finding it again next run
    _window = WindowDefault();
  }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }

  static SHParametersInfo parameters() { return SHParametersInfo(windowParams); }

  virtual void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _winName = value.payload.stringValue;
      break;
    case 1:
      _winClass = value.payload.stringValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      return shards::Var(_winName);
    case 1:
      return shards::Var(_winClass);
    default:
      break;
    }
    return res;
  }

protected:
  static inline shards::ParamsInfo windowParams = shards::ParamsInfo(
      shards::ParamsInfo::Param("Title", SHCCSTR("The title of the window to look for."), shards::CoreInfo::StringType),
      shards::ParamsInfo::Param("Class", SHCCSTR("An optional and platform dependent window class."),
                                shards::CoreInfo::StringType));

  static T WindowDefault();
  std::string _winName;
  std::string _winClass;
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
  static inline shards::ParamsInfo sizeParams =
      shards::ParamsInfo(shards::ParamsInfo::Param("Width", SHCCSTR("The desired width."), shards::CoreInfo::IntType),
                         shards::ParamsInfo::Param("Height", SHCCSTR("The desired height."), shards::CoreInfo::IntType));

  int _width;
  int _height;

  static SHParametersInfo parameters() { return SHParametersInfo(sizeParams); }

  virtual void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _width = value.payload.intValue;
      break;
    case 1:
      _height = value.payload.intValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      return shards::Var(_width);
    case 1:
      return shards::Var(_height);
    default:
      break;
    }
    return res;
  }
};

struct MoveWindowBase : public WinOpBase {
  static inline shards::ParamsInfo posParams = shards::ParamsInfo(
      shards::ParamsInfo::Param("X", SHCCSTR("The desired horizontal coordinates."), shards::CoreInfo::IntType),
      shards::ParamsInfo::Param("Y", SHCCSTR("The desired vertical coordinates."), shards::CoreInfo::IntType));

  int _x;
  int _y;

  static SHParametersInfo parameters() { return SHParametersInfo(posParams); }

  virtual void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _x = value.payload.intValue;
      break;
    case 1:
      _y = value.payload.intValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      return shards::Var(_x);
    case 1:
      return shards::Var(_y);
    default:
      break;
    }
    return res;
  }
};

struct SetTitleBase : public WinOpBase {
  static inline shards::ParamsInfo windowParams = shards::ParamsInfo(
      shards::ParamsInfo::Param("Title", SHCCSTR("The title of the window to look for."), shards::CoreInfo::StringType));

  std::string _title;

  static SHParametersInfo parameters() { return SHParametersInfo(windowParams); }

  SHVar getParam(int index) { return shards::Var(_title); }

  virtual void setParam(int index, const SHVar &value) { _title = value.payload.stringValue; }
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
  static inline shards::ParamsInfo params =
      shards::ParamsInfo(shards::ParamsInfo::Param("Window",
                                                   SHCCSTR("None or a window variable if we wish to send "
                                                           "the event only to a specific target window."),
                                                   Globals::windowVarOrNone));

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Int2Type; }

  static SHOptionalString help() {
    return SHCCSTR("### Sends the input key event.\n#### The input of this "
                   "shard will be a Int2.\n * The first integer will be 0 for "
                   "Key down/push events and 1 for Key up/release events.\n * "
                   "The second integer will the scancode of the key.\n");
  }

  std::string _windowVarName;
  shards::ExposedInfo _exposedInfo;

  SHExposedTypesInfo requiredVariables() {
    if (_windowVarName.size() == 0) {
      return {};
    } else {
      return SHExposedTypesInfo(_exposedInfo);
    }
  }

  void setParam(int index, const SHVar &value) {
    if (value.valueType == None) {
      _windowVarName.clear();
    } else {
      _windowVarName = value.payload.stringValue;
      _exposedInfo = shards::ExposedInfo(
          shards::ExposedInfo::Variable(_windowVarName.c_str(), SHCCSTR("The window to send events to."), Globals::windowType));
    }
  }

  SHVar getParam(int index) {
    if (_windowVarName.size() == 0) {
      return shards::Var::Empty;
    } else {
      return shards::Var(_windowVarName);
    }
  }
};

struct MousePosBase {
  shards::ParamVar _window{};
  shards::ExposedInfo _consuming{};

  static inline shards::ParamsInfo params = shards::ParamsInfo(shards::ParamsInfo::Param(
      "Window", SHCCSTR("None or a window variable we wish to use as relative origin."), Globals::windowVarOrNone));

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Int2Type; }

  SHExposedTypesInfo requiredVariables() {
    if (_window.isVariable()) {
      _consuming =
          shards::ExposedInfo(shards::ExposedInfo::Variable(_window.variableName(), SHCCSTR("The window."), Globals::windowType));
      return SHExposedTypesInfo(_consuming);
    } else {
      return {};
    }
  }

  void setParam(int index, const SHVar &value) { _window = value; }

  SHVar getParam(int index) { return _window; }

  void cleanup() { _window.cleanup(); }
  void warmup(SHContext *context) { _window.warmup(context); }
};

struct LastInputBase {
  // outputs the seconds since the last input happened
  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::FloatType; }
};
}; // namespace Desktop

#endif // SH_EXTRA_DESKTOP
