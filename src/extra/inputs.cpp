/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "SDL.h"
#include "blocks/shared.hpp"
#include "gfx.hpp"
#include <gfx/window.hpp>

using namespace linalg::aliases;

namespace chainblocks {
namespace Inputs {
struct Base : public gfx::BaseConsumer {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
};

struct MousePixelPos : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  CBTypeInfo compose(const CBInstanceData &data) {
    // no base call.. Await should be fine here
    return CoreInfo::Int2Type;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    int32_t mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    return Var(mouseX, mouseY);
  }

  void cleanup() {}
};

struct MouseDelta : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float2Type; }

  CBTypeInfo compose(const CBInstanceData &data) {
    composeCheckMainThread(data);
    return CoreInfo::Float2Type;
  }

  void warmup(CBContext *context) { baseConsumerWarmup(context); }

  void cleanup() { baseConsumerCleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    int2 windowSize = getWindow().getSize();

    for (auto &event : getMainWindowGlobals().events) {
      if (event.type == SDL_MOUSEMOTION) {
        return Var(float(event.motion.xrel) / float(windowSize.x),
                   float(event.motion.yrel) / float(windowSize.y));
      }
    }

    return Var(0.0, 0.0);
  }
};

struct MousePos : public Base {
  int _width;
  int _height;

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float2Type; }

  CBTypeInfo compose(const CBInstanceData &data) {
    composeCheckMainThread(data);
    return CoreInfo::Float2Type;
  }

  void warmup(CBContext *context) { baseConsumerWarmup(context); }

  void cleanup() { baseConsumerCleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    int2 windowSize = getWindow().getSize();

    int32_t mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    return Var(float(mouseX) / float(windowSize.x),
               float(mouseY) / float(windowSize.y));
  }
};

struct WindowSize : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  CBTypeInfo compose(const CBInstanceData &data) {
    composeCheckMainThread(data);
    return CoreInfo::Float2Type;
  }

  void warmup(CBContext *context) { baseConsumerWarmup(context); }

  void cleanup() { baseConsumerCleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    int2 windowSize = getWindow().getSize();
    return Var(windowSize.x, windowSize.y);
  }
};

struct Mouse : public Base {
  ParamVar _hidden{Var(false)};
  bool _isHidden{false};
  ParamVar _captured{Var(false)};
  bool _isCaptured{false};
  ParamVar _relative{Var(false)};
  bool _isRelative{false};
  SDL_Window *_capturedWindow{};

  static inline Parameters params{
      {"Hidden",
       CBCCSTR("If the cursor should be hidden."),
       {CoreInfo::BoolType}},
      {"Capture",
       CBCCSTR("If the mouse should be confined to the application window."),
       {CoreInfo::BoolType}},
      {"Relative",
       CBCCSTR("If the mouse should only report relative movements."),
       {CoreInfo::BoolType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _hidden = value;
      break;
    case 1:
      _captured = value;
      break;
    case 2:
      _relative = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _hidden;
    case 1:
      return _captured;
    case 2:
      return _relative;
    default:
      throw InvalidParameterIndex();
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    composeCheckMainThread(data);
    return data.inputType;
  }

  void warmup(CBContext *context) {
    _hidden.warmup(context);
    _captured.warmup(context);
    _relative.warmup(context);
    baseConsumerWarmup(context);
  }

  void cleanup() {
    _hidden.cleanup();
    _captured.cleanup();
    _relative.cleanup();

    setHidden(false);
    setCaptured(false);
    setRelative(false);
  }

  void setHidden(bool hidden) {
    if (hidden != _isHidden) {
      SDL_ShowCursor(hidden ? SDL_DISABLE : SDL_ENABLE);
      _isHidden = hidden;
    }
  }

  void setRelative(bool relative) {
    if (relative != _isRelative) {
      SDL_SetRelativeMouseMode(relative ? SDL_TRUE : SDL_FALSE);
      _isRelative = relative;
    }
  }

  void setCaptured(bool captured) {
    if (captured != _isCaptured) {
      SDL_Window *windowToCapture = getSdlWindow();
      SDL_SetWindowGrab(windowToCapture, captured ? SDL_TRUE : SDL_FALSE);
      _capturedWindow = captured ? windowToCapture : nullptr;
      _isCaptured = captured;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    setHidden(_hidden.get().payload.boolValue);
    setCaptured(_captured.get().payload.boolValue);
    setRelative(_relative.get().payload.boolValue);

    return input;
  }
};

template <SDL_EventType EVENT_TYPE> struct MouseUpDown : public Base {
  static inline Parameters params{
      {"Left",
       CBCCSTR(
           "The action to perform when the left mouse button is pressed down."),
       {CoreInfo::BlocksOrNone}},
      {"Right",
       CBCCSTR("The action to perform when the right mouse button is pressed "
               "down."),
       {CoreInfo::BlocksOrNone}},
      {"Middle",
       CBCCSTR("The action to perform when the middle mouse button is pressed "
               "down."),
       {CoreInfo::BlocksOrNone}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _leftButton = value;
      break;
    case 1:
      _rightButton = value;
      break;
    case 2:
      _middleButton = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _leftButton;
    case 1:
      return _rightButton;
    case 2:
      return _middleButton;
    default:
      throw InvalidParameterIndex();
    }
  }

  BlocksVar _leftButton{};
  BlocksVar _rightButton{};
  BlocksVar _middleButton{};

  CBTypeInfo compose(const CBInstanceData &data) {
    composeCheckMainThread(data);

    _leftButton.compose(data);
    _rightButton.compose(data);
    _middleButton.compose(data);

    return data.inputType;
  }

  void cleanup() {
    _leftButton.cleanup();
    _rightButton.cleanup();
    _middleButton.cleanup();
    baseConsumerCleanup();
  }

  void warmup(CBContext *context) {
    _leftButton.warmup(context);
    _rightButton.warmup(context);
    _middleButton.warmup(context);
    baseConsumerWarmup(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    for (auto &event : getMainWindowGlobals().events) {
      if (event.type == EVENT_TYPE) {
        CBVar output{};
        if (event.button.button == SDL_BUTTON_LEFT) {
          _leftButton.activate(context, input, output);
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
          _rightButton.activate(context, input, output);
        } else if (event.button.button == SDL_BUTTON_MIDDLE) {
          _middleButton.activate(context, input, output);
        }
      }
    }
    return input;
  }
};

using MouseUp = MouseUpDown<SDL_MOUSEBUTTONUP>;
using MouseDown = MouseUpDown<SDL_MOUSEBUTTONDOWN>;

template <SDL_EventType EVENT_TYPE> struct KeyUpDown : public Base {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _key.clear();
      } else {
        _key = value.payload.stringValue;
      }
      _keyCode = keyStringToKeyCode(_key);
    } break;
    case 1:
      _blocks = value;
      break;
    case 2:
      _repeat = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_key);
    case 1:
      return _blocks;
    case 2:
      return Var(_repeat);
    default:
      throw InvalidParameterIndex();
    }
  }

  void cleanup() {
    _blocks.cleanup();
    baseConsumerCleanup();
  }

  void warmup(CBContext *context) {
    _blocks.warmup(context);
    baseConsumerWarmup(context);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    composeCheckMainThread(data);

    _blocks.compose(data);

    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    for (auto &event : getMainWindowGlobals().events) {
      if (event.type == EVENT_TYPE && event.key.keysym.sym == _keyCode) {
        if (_repeat || event.key.repeat == 0) {
          CBVar output{};
          _blocks.activate(context, input, output);
        }
      }
    }

    return input;
  }

  static SDL_Keycode keyStringToKeyCode(const std::string &str) {
    if (str.length() == 1) {
      if ((str[0] >= ' ' && str[0] <= '@') ||
          (str[0] >= '[' && str[0] <= 'z')) {
        return SDL_Keycode(str[0]);
      }
      if (str[0] >= 'A' && str[0] <= 'Z') {
        return SDL_Keycode(str[0] + 32);
      }
    }

    auto it = _keyMap.find(str);
    if (it != _keyMap.end())
      return it->second;

    return SDLK_UNKNOWN;
  }

private:
  static inline Parameters _params = {
      {"Key", CBCCSTR("TODO!"), {{CoreInfo::StringType}}},
      {"Action", CBCCSTR("TODO!"), {CoreInfo::BlocksOrNone}},
      {"Repeat", CBCCSTR("TODO!"), {{CoreInfo::BoolType}}},
  };

  BlocksVar _blocks{};
  std::string _key;
  SDL_Keycode _keyCode;
  bool _repeat{false};

  static inline std::map<std::string, SDL_Keycode> _keyMap = {
      // note: keep the same order as in SDL_keycode.h
      {"return", SDLK_RETURN},
      {"escape", SDLK_ESCAPE},
      {"backspace", SDLK_BACKSPACE},
      {"tab", SDLK_TAB},
      {"space", SDLK_SPACE},
      {"f1", SDLK_F1},
      {"f2", SDLK_F2},
      {"f3", SDLK_F3},
      {"f4", SDLK_F4},
      {"f5", SDLK_F5},
      {"f6", SDLK_F6},
      {"f7", SDLK_F7},
      {"f8", SDLK_F8},
      {"f9", SDLK_F9},
      {"f10", SDLK_F10},
      {"f11", SDLK_F11},
      {"f12", SDLK_F12},
      {"pause", SDLK_PAUSE},
      {"insert", SDLK_INSERT},
      {"home", SDLK_HOME},
      {"pageUp", SDLK_PAGEUP},
      {"delete", SDLK_DELETE},
      {"end", SDLK_END},
      {"pageDown", SDLK_PAGEDOWN},
      {"right", SDLK_RIGHT},
      {"left", SDLK_LEFT},
      {"down", SDLK_DOWN},
      {"up", SDLK_UP},
      {"divide", SDLK_KP_DIVIDE},
      {"multiply", SDLK_KP_MULTIPLY},
      {"subtract", SDLK_KP_MINUS},
      {"add", SDLK_KP_PLUS},
      {"enter", SDLK_KP_ENTER},
      {"num1", SDLK_KP_1},
      {"num2", SDLK_KP_2},
      {"num3", SDLK_KP_3},
      {"num4", SDLK_KP_4},
      {"num5", SDLK_KP_5},
      {"num6", SDLK_KP_6},
      {"num7", SDLK_KP_7},
      {"num8", SDLK_KP_8},
      {"num9", SDLK_KP_9},
      {"num0", SDLK_KP_0},
      {"leftCtrl", SDLK_LCTRL},
      {"leftShift", SDLK_LSHIFT},
      {"leftAlt", SDLK_LALT},
      {"rightCtrl", SDLK_RCTRL},
      {"rightShift", SDLK_RSHIFT},
      {"rightAlt", SDLK_RALT},
  };
};

using KeyUp = KeyUpDown<SDL_KEYUP>;
using KeyDown = KeyUpDown<SDL_KEYDOWN>;

void registerBlocks() {
  REGISTER_CBLOCK("Window.Size", WindowSize);
  REGISTER_CBLOCK("Inputs.MousePixelPos", MousePixelPos);
  REGISTER_CBLOCK("Inputs.MousePos", MousePos);
  REGISTER_CBLOCK("Inputs.MouseDelta", MouseDelta);
  REGISTER_CBLOCK("Inputs.Mouse", Mouse);
  REGISTER_CBLOCK("Inputs.MouseUp", MouseUp);
  REGISTER_CBLOCK("Inputs.MouseDown", MouseDown);
  REGISTER_CBLOCK("Inputs.KeyUp", KeyUp);
  REGISTER_CBLOCK("Inputs.KeyDown", KeyDown);
}
} // namespace Inputs
} // namespace chainblocks