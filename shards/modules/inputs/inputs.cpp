/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "input/events.hpp"
#include "inputs.hpp"
#include "shards/linalg_shim.hpp"
#include <shards/shards.h>
#include <shards/shards.hpp>
#include <shards/core/shared.hpp>
#include <shards/input/master.hpp>
#include <shards/input/sdl.hpp>
#include <shards/modules/gfx/gfx.hpp>
#include <shards/modules/gfx/window.hpp>
#include <shards/core/platform.hpp>
#include <shards/input/log.hpp>
#include <shards/core/params.hpp>
#include <variant>
#include <boost/filesystem.hpp>

#if SH_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

using namespace linalg::aliases;

namespace shards {
namespace input {

enum ModifierKey {
  None,
  Shift,
  Alt,
  // Usually the control or cmd key on apple
  Primary,
  // Usually the windows key or control key on apple
  Secondary
};
DECL_ENUM_INFO(ModifierKey, ModifierKey, 'mdIf');

struct Types {
  static inline Type ModifierKeys = Type::SeqOf(ModifierKeyEnumInfo::Type);
};

static inline std::map<std::string, SDL_Keycode> KeycodeMap = {
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

inline bool matchModifiers(SDL_Keymod a, SDL_Keymod mask) {
  auto check = [&](SDL_Keymod mod) {
    if ((mask & mod) == mod) {
      if ((a & mod) == 0)
        return false;
    }
    return true;
  };
  if (!check(SDL_KMOD_CTRL))
    return false;
  if (!check(SDL_KMOD_ALT))
    return false;
  if (!check(SDL_KMOD_GUI))
    return false;
  if (!check(SDL_KMOD_SHIFT))
    return false;
  return true;
}

inline SDL_Keycode keyStringToKeyCode(const std::string &str) {
  if (str.empty()) {
    return SDLK_UNKNOWN;
  }

  if (str.length() == 1) {
    if ((str[0] >= ' ' && str[0] <= '@') || (str[0] >= '[' && str[0] <= 'z')) {
      return SDL_Keycode(str[0]);
    }
    if (str[0] >= 'A' && str[0] <= 'Z') {
      return SDL_Keycode(str[0] + 32);
    }
  }

  auto it = KeycodeMap.find(str);
  if (it != KeycodeMap.end())
    return it->second;

  throw SHException(fmt::format("Unknown key identifier: {}", str));
}

inline SDL_Keymod convertModifierKeys(const Var &modifiers) {
  SDL_Keymod result = SDL_KMOD_NONE;
  if (!modifiers.isNone()) {
    auto &seq = (SeqVar &)modifiers;
    for (auto &mod_ : seq) {
      auto &mod = (ModifierKey &)mod_.payload.enumValue;
      switch (mod) {
      case ModifierKey::Shift:
        (uint16_t &)result |= SDL_KMOD_SHIFT;
        break;
      case ModifierKey::Alt:
        (uint16_t &)result |= SDL_KMOD_ALT;
        break;
      case ModifierKey::Primary:
        (uint16_t &)result |= KMOD_PRIMARY;
        break;
      case ModifierKey::Secondary:
        (uint16_t &)result |= KMOD_SECONDARY;
        break;
      default:
        break;
      }
    }
  }
  return result;
}

struct Base {
  RequiredInputContext _inputContext;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void baseWarmup(SHContext *context) { _inputContext.warmup(context); }
  void baseCleanup(SHContext *context) { _inputContext.cleanup(); }
  void baseCompose(const SHInstanceData &data, ExposedInfo &requiredVariables) { _inputContext.compose(data, requiredVariables); }

  void warmup(SHContext *context) { baseWarmup(context); }
  void cleanup(SHContext *context) { baseCleanup(context); }
};

struct MousePixelPos : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    _requiredVariables.clear();
    baseCompose(data, _requiredVariables);
    return CoreInfo::Int2Type;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &state = _inputContext->getState();
    auto &region = state.region;
    float2 scale = float2(region.pixelSize) / region.size;
    return toVar(int2(state.cursorPosition * scale));
  }
};

struct MouseDelta : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float2Type; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    _requiredVariables.clear();
    baseCompose(data, _requiredVariables);
    return CoreInfo::Float2Type;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    float2 delta{};
    for (auto &event : _inputContext->getEvents()) {
      if (const PointerMoveEvent *pme = std::get_if<PointerMoveEvent>(&event.event)) {
        delta += pme->delta;
      }
    }
    return toVar(delta);
  }
};

struct MousePos : public Base {
  int _width;
  int _height;

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float2Type; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    _requiredVariables.clear();
    baseCompose(data, _requiredVariables);
    return CoreInfo::Float2Type;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    float2 cursorPosition = _inputContext->getState().cursorPosition;
    return toVar(cursorPosition);
  }
};

struct InputRegionSize : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float2Type; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    _requiredVariables.clear();
    baseCompose(data, _requiredVariables);
    return CoreInfo::Float2Type;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    float2 size = _inputContext->getState().region.size;
    return toVar(size);
  }
};

struct InputRegionPixelSize : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    _requiredVariables.clear();
    baseCompose(data, _requiredVariables);
    return CoreInfo::Int2Type;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    int2 size = (int2)_inputContext->getState().region.pixelSize;
    return toVar(size);
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
      {"Hidden", SHCCSTR("If the cursor should be hidden."), {CoreInfo::BoolType}},
      {"Capture", SHCCSTR("If the mouse should be confined to the application window."), {CoreInfo::BoolType}},
      {"Relative", SHCCSTR("If the mouse should only report relative movements."), {CoreInfo::BoolType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
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

  SHVar getParam(int index) {
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

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    _requiredVariables.clear();
    baseCompose(data, _requiredVariables);
    return data.inputType;
  }

  void warmup(SHContext *context) {
    _hidden.warmup(context);
    _captured.warmup(context);
    _relative.warmup(context);
    baseWarmup(context);
  }

  void cleanup(SHContext *context) {
    _hidden.cleanup();
    _captured.cleanup();
    _relative.cleanup();

    setHidden(false);
    setCaptured(false);
    setRelative(false);
  }

  void setHidden(bool hidden) {
    if (hidden != _isHidden) {
      // SDL_ShowCursor(hidden ? SDL_DISABLE : SDL_ENABLE);
      _isHidden = hidden;
    }
  }

  void setRelative(bool relative) {
    if (relative != _isRelative) {
      // SDL_SetRelativeMouseMode(relative ? SDL_TRUE : SDL_FALSE);
      _isRelative = relative;
    }
  }

  void setCaptured(bool captured) {
    if (captured != _isCaptured) {
      // SDL_Window *windowToCapture = _windowContext->getSdlWindow();
      // SDL_SetWindowGrab(windowToCapture, captured ? SDL_TRUE : SDL_FALSE);
      // _capturedWindow = captured ? windowToCapture : nullptr;
      _isCaptured = captured;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // setHidden(_hidden.get().payload.boolValue);
    // setCaptured(_captured.get().payload.boolValue);
    // setRelative(_relative.get().payload.boolValue);
    // TODO

    return input;
  }
};

template <bool Pressed> struct MouseUpDown : public Base {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM(ShardsVar, _leftButton, "Left", "The action to perform when the left mouse button is pressed down.",
        {CoreInfo::ShardsOrNone});
  PARAM(ShardsVar, _rightButton, "Right", "The action to perform when the right mouse button is pressed down.",
        {CoreInfo::ShardsOrNone});
  PARAM(ShardsVar, _middleButton, "Middle", "The action to perform when the middle mouse button is pressed down.",
        {CoreInfo::ShardsOrNone});
  PARAM_VAR(_consume, "Consume", "Consume events.", {CoreInfo::NoneType, CoreInfo::BoolType});
  PARAM_VAR(_ignoreConsumed, "IgnoreConsumed", "Ignore consumed events.", {CoreInfo::NoneType, CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_leftButton), PARAM_IMPL_FOR(_rightButton), PARAM_IMPL_FOR(_middleButton), PARAM_IMPL_FOR(_consume),
             PARAM_IMPL_FOR(_ignoreConsumed));

  bool ignoreConsumed{};
  bool consume{};

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
    ignoreConsumed = _ignoreConsumed->isNone() ? true : (bool)*_ignoreConsumed;
    consume = _consume->isNone() ? true : (bool)*_consume;
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    baseCompose(data, _requiredVariables);
    _leftButton.compose(data);
    _rightButton.compose(data);
    _middleButton.compose(data);
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto consume = _inputContext->getEventConsumer();

    if (!_inputContext->canReceiveInput())
      return input;

    // Use step counter to detect suspends and ignore
    size_t startingStep = context->stepCounter;
    for (auto &event : _inputContext->getEvents()) {
      if (ignoreConsumed && event.isConsumed())
        continue;

      if (const PointerButtonEvent *pbe = std::get_if<PointerButtonEvent>(&event.event)) {
        if (pbe->pressed == Pressed) {
          SHVar output{};
          if (pbe->index == SDL_BUTTON_LEFT) {
            if (_leftButton) {
              _leftButton.activate(context, input, output);
              if (startingStep != context->stepCounter)
                break;
              if (this->consume)
                consume(event);
            }
          } else if (pbe->index == SDL_BUTTON_RIGHT) {
            if (_rightButton) {
              _rightButton.activate(context, input, output);
              if (startingStep != context->stepCounter)
                break;
              if (this->consume)
                consume(event);
            }
          } else if (pbe->index == SDL_BUTTON_MIDDLE) {
            if (_middleButton) {
              _middleButton.activate(context, input, output);
              if (startingStep != context->stepCounter)
                break;
              if (this->consume)
                consume(event);
            }
          }
        }
      }
    }
    return input;
  }
};

template <bool Pressed> struct KeyUpDown : public Base {

  PARAM_VAR(_key, "Key", "The key to check.", {{CoreInfo::StringType}});
  PARAM(ShardsVar, _shards, "Action", "The Shards to run if a key event happened.", {CoreInfo::ShardsOrNone});
  PARAM_VAR(_repeat, "Repeat", "If the key event should be repeated.", {{CoreInfo::NoneType, CoreInfo::BoolType}});
  PARAM_VAR(_modifiers, "Modifiers", "Modifier keys to check.", {CoreInfo::NoneType, Types::ModifierKeys});
  PARAM_VAR(_consume, "Consume", "Consume events.", {CoreInfo::NoneType, CoreInfo::BoolType});
  PARAM_VAR(_ignoreConsumed, "IgnoreConsumed", "Ignore consumed events.", {CoreInfo::NoneType, CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_key), PARAM_IMPL_FOR(_shards), PARAM_IMPL_FOR(_repeat), PARAM_IMPL_FOR(_modifiers),
             PARAM_IMPL_FOR(_consume), PARAM_IMPL_FOR(_ignoreConsumed));

  SDL_Keymod _modifierMask;
  SDL_Keycode _keyCode;
  bool ignoreConsumed{};
  bool consume{};

public:
  KeyUpDown() { _repeat = Var(false); }

  void cleanup(SHContext *context) {
    _shards.cleanup(context);
    baseCleanup(context);
  }

  void warmup(SHContext *context) {
    _shards.warmup(context);
    baseWarmup(context);
    _modifierMask = convertModifierKeys(*_modifiers);
    _keyCode = keyStringToKeyCode(std::string(SHSTRVIEW(_key)));
    ignoreConsumed = _ignoreConsumed->isNone() ? true : (bool)*_ignoreConsumed;
    consume = _consume->isNone() ? true : (bool)*_consume;
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    baseCompose(data, _requiredVariables);
    _shards.compose(data);
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_inputContext->canReceiveInput())
      return input;

    // Use step counter to detect suspends and ignore
    size_t startingStep = context->stepCounter;
    auto &events = _inputContext->getEvents();
    auto consume = _inputContext->getEventConsumer();
    for (auto &event : events) {
      if (ignoreConsumed && event.isConsumed())
        continue;
      if (const KeyEvent *ke = std::get_if<KeyEvent>(&event.event)) {
        if (ke->pressed == Pressed && ke->key == _keyCode && matchModifiers(ke->modifiers, _modifierMask)) {
          if (*_repeat || ke->repeat == 0) {
            SHVar output{};
            _shards.activate(context, input, output);
            if (startingStep != context->stepCounter)
              break;
            if (this->consume)
              consume(event);
          }
        }
      }
    }

    return input;
  }
};

struct MatchModifier : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

  PARAM_VAR(_modifiers, "Modifiers", "Modifier keys to check.", {CoreInfo::NoneType, Types::ModifierKeys});
  PARAM_IMPL(PARAM_IMPL_FOR(_modifiers));

  SDL_Keymod _modifierMask;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.clear();
    baseCompose(data, _requiredVariables);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) {
    PARAM_WARMUP(context);
    baseCleanup(context);
  }
  void warmup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseWarmup(context);
    _modifierMask = convertModifierKeys(*_modifiers);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto modState = _inputContext->getState().modifiers;
    return Var(matchModifiers(modState, _modifierMask));
  }
};

struct IsKeyDown : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

  SDL_Keycode _keyCode;
  std::string _keyName;

  static inline Parameters _params = {
      {"Key", SHCCSTR("The name of the key to check."), {{CoreInfo::StringType}}},
  };

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == SHType::None) {
        _keyName.clear();
      } else {
        _keyName = SHSTRVIEW(value);
      }
      _keyCode = keyStringToKeyCode(_keyName);
    } break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_keyName);
    default:
      throw InvalidParameterIndex();
    }
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    _requiredVariables.clear();
    baseCompose(data, _requiredVariables);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) { baseCleanup(context); }
  void warmup(SHContext *context) { baseWarmup(context); }

  SHVar activate(SHContext *context, const SHVar &input) { return Var(_inputContext->getState().isKeyHeld(_keyCode)); }
};

struct HandleURL : public Base {
  std::string _url;

  PARAM(ShardsVar, _action, "Action", "The Shards to run if a text/file drop event happened.", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_action));

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    Base::cleanup(context);
  }

  void warmup(SHContext *context) {
    Base::warmup(context);

    PARAM_WARMUP(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    baseCompose(data, _requiredVariables);

    auto dataCopy = data;
    dataCopy.inputType = CoreInfo::StringType;
    _action.compose(dataCopy);
    return data.inputType;
  }

  static SHTypeInfo inputType() { return CoreInfo::AnyType; }
  static SHTypeInfo outputType() { return CoreInfo::AnyType; }

  std::vector<DropFileEvent> tmpEvents;

  SHVar activate(SHContext *context, const SHVar &input) {
    tmpEvents.clear();
    for (auto &evt : _inputContext->getEvents()) {
      if (const DropFileEvent *de = std::get_if<DropFileEvent>(&evt.event)) {
        tmpEvents.push_back(*de);
      }
    }

#if SH_EMSCRIPTEN
    // clang-format off
    const char* droppedPath = (const char*)MAIN_THREAD_EM_ASM_PTR({
      const dropHandler =  Module["fblDropHandler"];
      if(dropHandler.droppedDataPath && dropHandler.droppedDataPath !== dropHandler.lastCheckedDroppedDataPath) {
        console.log(`Drop handler queried for ${dropHandler.droppedDataPath}`);
        dropHandler.lastCheckedDroppedDataPath = dropHandler.droppedDataPath;
        return stringToNewUTF8(dropHandler.droppedDataPath);
      }
      return null;
    });
    // clang-format on

    if (droppedPath) {
      boost::filesystem::directory_iterator it(droppedPath);
      for (auto &sub : it) {
        auto str = sub.path().string();
        boost::container::string path(str.c_str());
        DropFileEvent evt;
        tmpEvents.push_back(DropFileEvent{.path = path});
        SPDLOG_LOGGER_DEBUG(shards::input::getLogger(), "Dropped file: {}", path);
      }
      free((void *)droppedPath);
    }
#endif

    for (auto &evt : tmpEvents) {
      SHVar dummy{};
      SHWireState res = _action.activate(context, Var(evt.path.c_str(), 0 /* explicit strlen */), dummy);
      if (res == SHWireState::Error) {
        throw ActivationError("Inner activation failed");
      }
    }
    tmpEvents.clear();

    return input;
  }
};

struct DiscardTempFiles : public Base {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Discard temporary files created by open & drag-drop operations"); }

  PARAM_IMPL();

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    Base::cleanup(context);
  }

  void warmup(SHContext *context) {
    Base::warmup(context);
    PARAM_WARMUP(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    baseCompose(data, _requiredVariables);
    return data.inputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
#if SH_EMSCRIPTEN
    // clang-format off
    MAIN_THREAD_ASYNC_EM_ASM({
      const dropHandler =  Module["fblDropHandler"];
      dropHandler.discardData();
    });
// clang-format on
#endif
    return input;
  }
};

} // namespace input
} // namespace shards

SHARDS_REGISTER_FN(inputs) {
  using namespace shards::input;

  REGISTER_ENUM(ModifierKeyEnumInfo);

  REGISTER_SHARD("Inputs.Size", InputRegionSize);
  REGISTER_SHARD("Inputs.PixelSize", InputRegionPixelSize);
  REGISTER_SHARD("Inputs.MousePixelPos", MousePixelPos);
  REGISTER_SHARD("Inputs.MousePos", MousePos);
  REGISTER_SHARD("Inputs.MouseDelta", MouseDelta);
  REGISTER_SHARD("Inputs.Mouse", Mouse);
  REGISTER_SHARD("Inputs.IsKeyDown", IsKeyDown);
  REGISTER_SHARD("Inputs.HandleURL", HandleURL);
  REGISTER_SHARD("Inputs.DiscardTempFiles", DiscardTempFiles);

  using MouseDown = MouseUpDown<true>;
  using MouseUp = MouseUpDown<false>;
  REGISTER_SHARD("Inputs.MouseDown", MouseDown);
  REGISTER_SHARD("Inputs.MouseUp", MouseUp);

  using KeyDown = KeyUpDown<true>;
  using KeyUp = KeyUpDown<false>;
  REGISTER_SHARD("Inputs.KeyDown", KeyDown);
  REGISTER_SHARD("Inputs.KeyUp", KeyUp);

  REGISTER_SHARD("Inputs.MatchModifier", MatchModifier);
}
