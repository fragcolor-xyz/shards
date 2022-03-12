/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#pragma once

#include "blocks/shared.hpp"
#include "runtime.hpp"
#include <cstdlib>

namespace Desktop {
constexpr uint32_t windowCC = 'hwnd';

struct Globals {
  static inline chainblocks::Type windowType{{CBType::Object, {.object = {.vendorId = chainblocks::CoreCC, .typeId = windowCC}}}};
  static inline chainblocks::Types windowVarOrNone{{windowType, chainblocks::CoreInfo::NoneType}};
};

template <typename T> class WindowBase {
public:
  void cleanup() {
    // reset to default
    // force finding it again next run
    _window = WindowDefault();
  }

  static CBTypesInfo inputTypes() { return chainblocks::CoreInfo::NoneType; }

  static CBParametersInfo parameters() { return CBParametersInfo(windowParams); }

  virtual void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      return chainblocks::Var(_winName);
    case 1:
      return chainblocks::Var(_winClass);
    default:
      break;
    }
    return res;
  }

protected:
  static inline chainblocks::ParamsInfo windowParams = chainblocks::ParamsInfo(
      chainblocks::ParamsInfo::Param("Title", CBCCSTR("The title of the window to look for."), chainblocks::CoreInfo::StringType),
      chainblocks::ParamsInfo::Param("Class", CBCCSTR("An optional and platform dependent window class."),
                                     chainblocks::CoreInfo::StringType));

  static T WindowDefault();
  std::string _winName;
  std::string _winClass;
  T _window;
};

struct ActiveBase {
  static CBTypesInfo inputTypes() { return Globals::windowType; }
  static CBTypesInfo outputTypes() { return chainblocks::CoreInfo::BoolType; }
};

struct PIDBase {
  static CBTypesInfo inputTypes() { return Globals::windowType; }
  static CBTypesInfo outputTypes() { return chainblocks::CoreInfo::IntType; }
};

struct WinOpBase {
  static CBTypesInfo inputTypes() { return Globals::windowType; }
  static CBTypesInfo outputTypes() { return Globals::windowType; }
};

struct SizeBase {
  static CBTypesInfo inputTypes() { return Globals::windowType; }
  static CBTypesInfo outputTypes() { return chainblocks::CoreInfo::Int2Type; }
};

struct ResizeWindowBase : public WinOpBase {
  static inline chainblocks::ParamsInfo sizeParams = chainblocks::ParamsInfo(
      chainblocks::ParamsInfo::Param("Width", CBCCSTR("The desired width."), chainblocks::CoreInfo::IntType),
      chainblocks::ParamsInfo::Param("Height", CBCCSTR("The desired height."), chainblocks::CoreInfo::IntType));

  int _width;
  int _height;

  static CBParametersInfo parameters() { return CBParametersInfo(sizeParams); }

  virtual void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      return chainblocks::Var(_width);
    case 1:
      return chainblocks::Var(_height);
    default:
      break;
    }
    return res;
  }
};

struct MoveWindowBase : public WinOpBase {
  static inline chainblocks::ParamsInfo posParams = chainblocks::ParamsInfo(
      chainblocks::ParamsInfo::Param("X", CBCCSTR("The desired horizontal coordinates."), chainblocks::CoreInfo::IntType),
      chainblocks::ParamsInfo::Param("Y", CBCCSTR("The desired vertical coordinates."), chainblocks::CoreInfo::IntType));

  int _x;
  int _y;

  static CBParametersInfo parameters() { return CBParametersInfo(posParams); }

  virtual void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      return chainblocks::Var(_x);
    case 1:
      return chainblocks::Var(_y);
    default:
      break;
    }
    return res;
  }
};

struct SetTitleBase : public WinOpBase {
  static inline chainblocks::ParamsInfo windowParams = chainblocks::ParamsInfo(chainblocks::ParamsInfo::Param(
      "Title", CBCCSTR("The title of the window to look for."), chainblocks::CoreInfo::StringType));

  std::string _title;

  static CBParametersInfo parameters() { return CBParametersInfo(windowParams); }

  CBVar getParam(int index) { return chainblocks::Var(_title); }

  virtual void setParam(int index, const CBVar &value) { _title = value.payload.stringValue; }
};

struct WaitKeyEventBase {
  static CBTypesInfo inputTypes() { return chainblocks::CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return chainblocks::CoreInfo::Int2Type; }

  static CBOptionalString help() {
    return CBCCSTR("### Pauses the chain and waits for keyboard events.\n#### The output "
                   "of this block will be a Int2.\n * The first integer will be 0 for Key "
                   "down/push events and 1 for Key up/release events.\n * The second "
                   "integer will the scancode of the key.\n");
  }
};

struct SendKeyEventBase {
  static inline chainblocks::ParamsInfo params =
      chainblocks::ParamsInfo(chainblocks::ParamsInfo::Param("Window",
                                                             CBCCSTR("None or a window variable if we wish to send "
                                                                     "the event only to a specific target window."),
                                                             Globals::windowVarOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return chainblocks::CoreInfo::Int2Type; }
  static CBTypesInfo outputTypes() { return chainblocks::CoreInfo::Int2Type; }

  static CBOptionalString help() {
    return CBCCSTR("### Sends the input key event.\n#### The input of this "
                   "block will be a Int2.\n * The first integer will be 0 for "
                   "Key down/push events and 1 for Key up/release events.\n * "
                   "The second integer will the scancode of the key.\n");
  }

  std::string _windowVarName;
  chainblocks::ExposedInfo _exposedInfo;

  CBExposedTypesInfo requiredVariables() {
    if (_windowVarName.size() == 0) {
      return {};
    } else {
      return CBExposedTypesInfo(_exposedInfo);
    }
  }

  void setParam(int index, const CBVar &value) {
    if (value.valueType == None) {
      _windowVarName.clear();
    } else {
      _windowVarName = value.payload.stringValue;
      _exposedInfo = chainblocks::ExposedInfo(chainblocks::ExposedInfo::Variable(
          _windowVarName.c_str(), CBCCSTR("The window to send events to."), Globals::windowType));
    }
  }

  CBVar getParam(int index) {
    if (_windowVarName.size() == 0) {
      return chainblocks::Var::Empty;
    } else {
      return chainblocks::Var(_windowVarName);
    }
  }
};

struct MousePosBase {
  chainblocks::ParamVar _window{};
  chainblocks::ExposedInfo _consuming{};

  static inline chainblocks::ParamsInfo params = chainblocks::ParamsInfo(chainblocks::ParamsInfo::Param(
      "Window", CBCCSTR("None or a window variable we wish to use as relative origin."), Globals::windowVarOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return chainblocks::CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return chainblocks::CoreInfo::Int2Type; }

  CBExposedTypesInfo requiredVariables() {
    if (_window.isVariable()) {
      _consuming = chainblocks::ExposedInfo(
          chainblocks::ExposedInfo::Variable(_window.variableName(), CBCCSTR("The window."), Globals::windowType));
      return CBExposedTypesInfo(_consuming);
    } else {
      return {};
    }
  }

  void setParam(int index, const CBVar &value) { _window = value; }

  CBVar getParam(int index) { return _window; }

  void cleanup() { _window.cleanup(); }
  void warmup(CBContext *context) { _window.warmup(context); }
};

struct LastInputBase {
  // outputs the seconds since the last input happened
  static CBTypesInfo inputTypes() { return chainblocks::CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return chainblocks::CoreInfo::FloatType; }
};
}; // namespace Desktop
