/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#pragma once

#include "blocks/shared.hpp"
#include "runtime.hpp"
#include <cstdlib>

using namespace chainblocks;

namespace Desktop {
constexpr uint32_t windowCC = 'hwnd';

struct Globals {
  static inline Type windowType{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = windowCC}}}};
  static inline Types windowVarOrNone{{windowType, CoreInfo::NoneType}};
};

template <typename T> class WindowBase {
public:
  void cleanup() {
    // reset to default
    // force finding it again next run
    _window = WindowDefault();
  }

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBParametersInfo parameters() {
    return CBParametersInfo(windowParams);
  }

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
      return Var(_winName);
    case 1:
      return Var(_winClass);
    default:
      break;
    }
    return res;
  }

protected:
  static inline ParamsInfo windowParams = ParamsInfo(
      ParamsInfo::Param("Title",
                        CBCCSTR("The title of the window to look for."),
                        CoreInfo::StringType),
      ParamsInfo::Param(
          "Class", CBCCSTR("An optional and platform dependent window class."),
          CoreInfo::StringType));

  static T WindowDefault();
  std::string _winName;
  std::string _winClass;
  T _window;
};

struct ActiveBase {
  static CBTypesInfo inputTypes() { return Globals::windowType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
};

struct PIDBase {
  static CBTypesInfo inputTypes() { return Globals::windowType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }
};

struct WinOpBase {
  static CBTypesInfo inputTypes() { return Globals::windowType; }
  static CBTypesInfo outputTypes() { return Globals::windowType; }
};

struct SizeBase {
  static CBTypesInfo inputTypes() { return Globals::windowType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Int2Type; }
};

struct ResizeWindowBase : public WinOpBase {
  static inline ParamsInfo sizeParams =
      ParamsInfo(ParamsInfo::Param("Width", CBCCSTR("The desired width."),
                                   CoreInfo::IntType),
                 ParamsInfo::Param("Height", CBCCSTR("The desired height."),
                                   CoreInfo::IntType));

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
      return Var(_width);
    case 1:
      return Var(_height);
    default:
      break;
    }
    return res;
  }
};

struct MoveWindowBase : public WinOpBase {
  static inline ParamsInfo posParams = ParamsInfo(
      ParamsInfo::Param("X", CBCCSTR("The desired horizontal coordinates."),
                        CoreInfo::IntType),
      ParamsInfo::Param("Y", CBCCSTR("The desired vertical coordinates."),
                        CoreInfo::IntType));

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
      return Var(_x);
    case 1:
      return Var(_y);
    default:
      break;
    }
    return res;
  }
};

struct SetTitleBase : public WinOpBase {
  static inline ParamsInfo windowParams = ParamsInfo(ParamsInfo::Param(
      "Title", CBCCSTR("The title of the window to look for."),
      CoreInfo::StringType));

  std::string _title;

  static CBParametersInfo parameters() {
    return CBParametersInfo(windowParams);
  }

  CBVar getParam(int index) { return Var(_title); }

  virtual void setParam(int index, const CBVar &value) {
    _title = value.payload.stringValue;
  }
};

struct WaitKeyEventBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  static CBOptionalString help() {
    return CBCCSTR(
        "### Pauses the chain and waits for keyboard events.\n#### The output "
        "of this block will be a Int2.\n * The first integer will be 0 for Key "
        "down/push events and 1 for Key up/release events.\n * The second "
        "integer will the scancode of the key.\n");
  }
};

struct SendKeyEventBase {
  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Window",
                        CBCCSTR("None or a window variable if we wish to send "
                                "the event only to a specific target window."),
                        Globals::windowVarOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static CBTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  static CBOptionalString help() {
    return CBCCSTR("### Sends the input key event.\n#### The input of this "
                   "block will be a Int2.\n * The first integer will be 0 for "
                   "Key down/push events and 1 for Key up/release events.\n * "
                   "The second integer will the scancode of the key.\n");
  }

  std::string _windowVarName;
  ExposedInfo _exposedInfo;

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
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(
          _windowVarName.c_str(), CBCCSTR("The window to send events to."),
          Globals::windowType));
    }
  }

  CBVar getParam(int index) {
    if (_windowVarName.size() == 0) {
      return Var::Empty;
    } else {
      return Var(_windowVarName);
    }
  }
};

struct MousePosBase {
  ParamVar _window{};
  ExposedInfo _consuming{};

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Window",
      CBCCSTR("None or a window variable we wish to use as relative origin."),
      Globals::windowVarOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  CBExposedTypesInfo requiredVariables() {
    if (_window.isVariable()) {
      _consuming = ExposedInfo(ExposedInfo::Variable(
          _window.variableName(), CBCCSTR("The window."), Globals::windowType));
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
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }
};
}; // namespace Desktop
