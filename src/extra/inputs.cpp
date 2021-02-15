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

struct MousePos : public Base {
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
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float2Type; }

  CBTypeInfo compose(const CBInstanceData &data) {
    // no base call.. Await should be fine here
    return CoreInfo::Float2Type;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return Var(0.0, 0.0);
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

void registerBlocks() {
  REGISTER_CBLOCK("Inputs.MousePos", MousePos);
  REGISTER_CBLOCK("Inputs.Mouse", Mouse);
}
} // namespace Inputs
} // namespace chainblocks