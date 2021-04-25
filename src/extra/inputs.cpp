/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "./bgfx.hpp"
#include "SDL.h"
#include "blocks/shared.hpp"

namespace chainblocks {
namespace Inputs {
struct Base {
  static inline CBExposedTypeInfo ContextInfo = ExposedInfo::Variable(
      "GFX.CurrentWindow", CBCCSTR("The required SDL window."),
      BGFX::BaseConsumer::windowType);
  static inline ExposedInfo requiredInfo = ExposedInfo(ContextInfo);

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(requiredInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError(
          "Inputs Blocks cannot be used on a worker thread (e.g. "
          "within an Await block)");
    }
    return CoreInfo::NoneType; // on purpose to trigger assertion during
                               // validation
  }
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
};

struct MouseDelta : public Base {
  CBVar *_sdlWinVar{nullptr};
  Uint32 _windowId{0};
  int _width;
  int _height;

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float2Type; }

  CBTypeInfo compose(const CBInstanceData &data) {
    // sdlEvents is not thread safe
    Base::compose(data);
    return CoreInfo::Float2Type;
  }

  void warmup(CBContext *context) {
    _sdlWinVar = referenceVariable(context, "GFX.CurrentWindow");
    const auto window =
        reinterpret_cast<SDL_Window *>(_sdlWinVar->payload.objectValue);
    _windowId = SDL_GetWindowID(window);
    SDL_GetWindowSize(window, &_width, &_height);
  }

  void cleanup() {
    if (_sdlWinVar) {
      releaseVariable(_sdlWinVar);
      _sdlWinVar = nullptr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    for (const auto &event : BGFX::Context::sdlEvents) {
      if (event.type == SDL_MOUSEMOTION && event.motion.windowID == _windowId) {
        return Var(float(event.motion.xrel) / float(_width),
                   float(event.motion.yrel) / float(_height));
      } else if (event.type == SDL_WINDOWEVENT &&
                 event.window.event == SDL_WINDOWEVENT_RESIZED &&
                 event.window.windowID == _windowId) {
        const auto window =
            reinterpret_cast<SDL_Window *>(_sdlWinVar->payload.objectValue);
        SDL_GetWindowSize(window, &_width, &_height);
      }
    }
    return Var(0.0, 0.0);
  }
};

struct MousePos : public Base {
  CBVar *_sdlWinVar{nullptr};
  Uint32 _windowId{0};
  int _width;
  int _height;

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float2Type; }

  CBTypeInfo compose(const CBInstanceData &data) {
    // sdlEvents is not thread safe
    Base::compose(data);
    return CoreInfo::Float2Type;
  }

  void warmup(CBContext *context) {
    _sdlWinVar = referenceVariable(context, "GFX.CurrentWindow");
    const auto window =
        reinterpret_cast<SDL_Window *>(_sdlWinVar->payload.objectValue);
    _windowId = SDL_GetWindowID(window);
    SDL_GetWindowSize(window, &_width, &_height);
  }

  void cleanup() {
    if (_sdlWinVar) {
      releaseVariable(_sdlWinVar);
      _sdlWinVar = nullptr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    for (const auto &event : BGFX::Context::sdlEvents) {
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_RESIZED &&
          event.window.windowID == _windowId) {
        const auto window =
            reinterpret_cast<SDL_Window *>(_sdlWinVar->payload.objectValue);
        SDL_GetWindowSize(window, &_width, &_height);
      }
    }

    int32_t mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    return Var(float(mouseX) / float(_width), float(mouseY) / float(_height));
  }
};

struct WindowSize : public Base {
  CBVar *_sdlWinVar{nullptr};
  Uint32 _windowId{0};
  int _width;
  int _height;

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  CBTypeInfo compose(const CBInstanceData &data) {
    // sdlEvents is not thread safe
    Base::compose(data);
    return CoreInfo::Int2Type;
  }

  void warmup(CBContext *context) {
    _sdlWinVar = referenceVariable(context, "GFX.CurrentWindow");
    const auto window =
        reinterpret_cast<SDL_Window *>(_sdlWinVar->payload.objectValue);
    _windowId = SDL_GetWindowID(window);
    SDL_GetWindowSize(window, &_width, &_height);
  }

  void cleanup() {
    if (_sdlWinVar) {
      releaseVariable(_sdlWinVar);
      _sdlWinVar = nullptr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    for (const auto &event : BGFX::Context::sdlEvents) {
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_RESIZED &&
          event.window.windowID == _windowId) {
        const auto window =
            reinterpret_cast<SDL_Window *>(_sdlWinVar->payload.objectValue);
        SDL_GetWindowSize(window, &_width, &_height);
      }
    }
    return Var(_width, _height);
  }
};

struct Mouse : public Base {
  ParamVar _hidden{Var(false)};
  bool _isHidden{false};
  ParamVar _captured{Var(false)};
  bool _isCaptured{false};
  SDL_Window *_capturedWindow{nullptr};
  ParamVar _relative{Var(false)};
  bool _isRelative{false};
  CBVar *_sdlWinVar{nullptr};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

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
    // no base call.. Await should be fine here
    return data.inputType;
  }

  void warmup(CBContext *context) {
    _hidden.warmup(context);
    _captured.warmup(context);
    _relative.warmup(context);
    _sdlWinVar = referenceVariable(context, "GFX.CurrentWindow");
  }

  void cleanup() {
    _hidden.cleanup();
    _captured.cleanup();
    _relative.cleanup();
    if (_sdlWinVar) {
      releaseVariable(_sdlWinVar);
      _sdlWinVar = nullptr;
    }

    if (_isHidden) {
      SDL_ShowCursor(SDL_ENABLE);
    }
    _isHidden = false;
    if (_isCaptured && _capturedWindow) {
      SDL_SetWindowGrab(_capturedWindow, SDL_FALSE);
    }
    _isCaptured = false;
    if (_isRelative) {
      SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    _isRelative = false;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto window =
        reinterpret_cast<SDL_Window *>(_sdlWinVar->payload.objectValue);

    const auto hidden = _hidden.get().payload.boolValue;
    if (hidden && !_isHidden) {
      SDL_ShowCursor(SDL_DISABLE);
      _isHidden = true;
    } else if (!hidden && _isHidden) {
      SDL_ShowCursor(SDL_ENABLE);
      _isHidden = false;
    }

    const auto captured = _captured.get().payload.boolValue;
    if (captured && !_isCaptured) {
      SDL_SetWindowGrab(window, SDL_TRUE);
      _isCaptured = true;
      _capturedWindow = window;
    } else if (!captured && _isCaptured) {
      SDL_SetWindowGrab(window, SDL_FALSE);
      _isCaptured = false;
      _capturedWindow = nullptr;
    }

    const auto relative = _relative.get().payload.boolValue;
    if (relative && !_isRelative) {
      SDL_SetRelativeMouseMode(SDL_TRUE);
      _isRelative = true;
    } else if (!relative && _isRelative) {
      SDL_SetRelativeMouseMode(SDL_FALSE);
      _isRelative = false;
    }

    return input;
  }
};

template <SDL_EventType EVENT_TYPE> struct MouseUpDown : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

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
    // sdlEvents is not thread safe
    Base::compose(data);

    _leftButton.compose(data);
    _rightButton.compose(data);
    _middleButton.compose(data);

    return data.inputType;
  }

  void cleanup() {
    _leftButton.cleanup();
    _rightButton.cleanup();
    _middleButton.cleanup();
  }

  void warmup(CBContext *context) {
    _leftButton.warmup(context);
    _rightButton.warmup(context);
    _middleButton.warmup(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    for (const auto &event : BGFX::Context::sdlEvents) {
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

void registerBlocks() {
  REGISTER_CBLOCK("Window.Size", WindowSize);
  REGISTER_CBLOCK("Inputs.MousePixelPos", MousePixelPos);
  REGISTER_CBLOCK("Inputs.MousePos", MousePos);
  REGISTER_CBLOCK("Inputs.MouseDelta", MouseDelta);
  REGISTER_CBLOCK("Inputs.Mouse", Mouse);
  REGISTER_CBLOCK("Inputs.MouseUp", MouseUp);
  REGISTER_CBLOCK("Inputs.MouseDown", MouseDown);
}
} // namespace Inputs
} // namespace chainblocks