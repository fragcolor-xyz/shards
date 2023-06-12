#include "gfx.hpp"
#include "window.hpp"
#include <shards/linalg_shim.hpp>
#include <shards/core/params.hpp>
#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>
#include <gfx/context.hpp>
#include <gfx/window.hpp>

using namespace shards;

namespace shards {
void WindowContext::nextFrame() {}
gfx::Window &WindowContext::getWindow() { return *window.get(); }
SDL_Window *WindowContext::getSdlWindow() { return getWindow().window; }
} // namespace shards

namespace gfx {
struct MainWindow final {
  PARAM(OwnedVar, _title, "Title", "The title of the window to create.", {CoreInfo::StringType});
  PARAM(OwnedVar, _width, "Width", "The width of the window to create. In pixels and DPI aware.", {CoreInfo::IntType});
  PARAM(OwnedVar, _height, "Height", "The height of the window to create. In pixels and DPI aware.", {CoreInfo::IntType});
  PARAM(ShardsVar, _contents, "Contents", "The main input loop of this window.", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_title), PARAM_IMPL_FOR(_width), PARAM_IMPL_FOR(_height), PARAM_IMPL_FOR(_contents));

  static inline Type OutputType = Type(WindowContext::Type);

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return OutputType; }

  MainWindow() {
    _width = Var(1280);
    _height = Var(1280);
    _title = Var("Shards Window");
  }

  Window _window;

  WindowContext _windowContext;
  SHVar *_windowContextVar{};

  ExposedInfo _innerExposedVariables;
  ExposedInfo _exposedVariables;

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposedVariables); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _exposedVariables.clear();

    // Make sure that windows are UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, WindowContext::VariableName) == 0) {
        throw SHException("GFX.MainWindow must be unique, found another use!");
      }
    }

    _innerExposedVariables = ExposedInfo(data.shared);
    _innerExposedVariables.push_back(WindowContext::VariableInfo);

    SHInstanceData innerData = data;
    innerData.shared = SHExposedTypesInfo(_innerExposedVariables);
    _contents.compose(innerData);

    mergeIntoExposedInfo(_exposedVariables, _contents.composeResult().exposedInfo);

    // Merge required, but without the context variables
    for (auto &required : _contents.composeResult().requiredInfo) {
      std::string_view varName(required.name);
      if (varName == WindowContext::VariableName)
        continue;
      _requiredVariables.push_back(required);
    }

    return CoreInfo::NoneType;
  }

  void initWindow() {
    SHLOG_DEBUG("Creating window");

    WindowCreationOptions windowOptions = {};
    windowOptions.width = (int)Var(_width);
    windowOptions.height = (int)Var(_height);
    windowOptions.title = SHSTRVIEW(Var(_title));
    _windowContext.window = std::make_shared<Window>();
    _windowContext.window->init(windowOptions);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _windowContextVar = referenceVariable(context, WindowContext::VariableName);
    assignVariableValue(*_windowContextVar, Var::Object(&_windowContext, WindowContext::Type));

    _contents.warmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();

    if (_windowContext.window) {
      SHLOG_DEBUG("Destroying window");
      _windowContext.window->cleanup();
    }
    _windowContext.reset();

    if (_windowContextVar) {
      if (_windowContextVar->refcount > 1) {
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to {}", _windowContextVar->refcount - 1,
                    WindowContext::VariableName);
      }
      releaseVariable(_windowContextVar);
      _windowContextVar = nullptr;
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (!_windowContext.window) {
      initWindow();
    }

    auto &window = _windowContext.window;
    // auto &events = _windowContext.events;

    // window->pollEvents(events);
    // for (auto &event : events) {
    //   if (event.type == SDL_QUIT) {
    //     throw ActivationError("Window closed, aborting wire.");
    //   } else if (event.type == SDL_WINDOWEVENT) {
    //     if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
    //       throw ActivationError("Window closed, aborting wire.");
    //     } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
    //     }
    //   } else if (event.type == SDL_APP_DIDENTERBACKGROUND) {
    //     // context->suspend();
    //   } else if (event.type == SDL_APP_DIDENTERFOREGROUND) {
    //     // context->resume();
    //   }
    // }

    // Push root input region
    // auto &inputStack = _windowContext.inputStack;
    // inputStack.reset();
    // inputStack.push(input::InputStack::Item{
    //     .windowMapping = input::WindowSubRegion::fromEntireWindow(*window.get()),
    // });

    _windowContext.inputMaster.update(*window.get());

    SHVar _shardsOutput{};
    _contents.activate(shContext, input, _shardsOutput);

    // inputStack.pop();

    return *_windowContextVar;
  }
};

struct WindowSize {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_IMPL();

  RequiredWindowContext _requiredWindowContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.push_back(RequiredGraphicsContext::getExposedTypeInfo());
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredWindowContext.warmup(context);
  }
  void cleanup() {
    PARAM_CLEANUP();
    _requiredWindowContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return toVar(_requiredWindowContext->window->getSize()); }
};

struct ResizeWindow {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_IMPL();

  RequiredWindowContext _requiredWindowContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.push_back(decltype(_requiredWindowContext)::getExposedTypeInfo());
    return data.inputType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredWindowContext.warmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _requiredWindowContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _requiredWindowContext->window->resize(toInt2(input));
    return input;
  }
};
struct WindowPosition {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_IMPL();

  RequiredWindowContext _requiredWindowContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.push_back(decltype(_requiredWindowContext)::getExposedTypeInfo());
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredWindowContext.warmup(context);
  }
  void cleanup() {
    PARAM_CLEANUP();
    _requiredWindowContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return toVar(_requiredWindowContext->window->getPosition()); }
};

struct MoveWindow {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_IMPL();

  RequiredWindowContext _requiredWindowContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.push_back(decltype(_requiredWindowContext)::getExposedTypeInfo());
    return data.inputType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredWindowContext.warmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _requiredWindowContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _requiredWindowContext->window->move(toInt2(input));
    return input;
  }
};

void registerMainWindowShards() {
  REGISTER_SHARD("GFX.MainWindow", MainWindow);
  REGISTER_SHARD("GFX.WindowSize", WindowSize);
  REGISTER_SHARD("GFX.ResizeWindow", ResizeWindow);
  REGISTER_SHARD("GFX.WindowPosition", WindowPosition);
  REGISTER_SHARD("GFX.MoveWindow", MoveWindow);
}

} // namespace gfx
