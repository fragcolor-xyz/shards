#include "../gfx.hpp"
#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>
#include "../inputs.hpp"
#include "extra/gfx.hpp"
#include "foundation.hpp"
#include "gfx/platform_surface.hpp"
#include "linalg_shim.hpp"
#include "params.hpp"

using namespace shards;
using shards::Inputs::InputContext;

namespace gfx {

struct MainWindow final {
  static inline Parameters params{
      {"Title", SHCCSTR("The title of the window to create."), {CoreInfo::StringType}},
      {"Width", SHCCSTR("The width of the window to create. In pixels and DPI aware."), {CoreInfo::IntType}},
      {"Height", SHCCSTR("The height of the window to create. In pixels and DPI aware."), {CoreInfo::IntType}},
      {"Contents", SHCCSTR("The contents of this window."), {CoreInfo::ShardsOrNone}},
      {"Debug",
       SHCCSTR("If the device backing the window should be created with "
               "debug layers on."),
       {CoreInfo::BoolType}},
      {"IgnoreCompilationErrors",
       SHCCSTR("When enabled, shader or pipeline compilation errors will be ignored and either use fallback rendering or not "
               "render at all."),
       {CoreInfo::BoolType}},
  };

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHParametersInfo parameters() { return params; }

  std::string _title;
  int _pwidth = 1280;
  int _pheight = 720;
  bool _debug = false;
  Window _window;
  ShardsVar _shards;
  bool _ignoreCompilationErrors = false;

  InputContext _inputContext;
  SHVar *_inputContextVar{};

  GraphicsContext _graphicsContext;
  SHVar *_graphicsContextVar{};

  GraphicsRendererContext _graphicsRendererContext;
  SHVar *_graphicsRendererContextVar{};

  ::gfx::Loop _loop;

  ExposedInfo _exposedVariables;

  boost::container::flat_set<SDL_Keycode> _actuallyHeldKeys;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      break;
    case 1:
      _pwidth = int(value.payload.intValue);
      break;
    case 2:
      _pheight = int(value.payload.intValue);
      break;
    case 3:
      _shards = value;
      break;
    case 4:
      _debug = value.payload.boolValue;
      break;
    case 5:
      _ignoreCompilationErrors = value.payload.boolValue;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_title);
    case 1:
      return Var(_pwidth);
    case 2:
      return Var(_pheight);
    case 3:
      return _shards;
    case 4:
      return Var(_debug);
    case 5:
      return Var(_ignoreCompilationErrors);
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    // Require extra stack space for the wire containing the main window
    data.wire->stackSize = std::max<size_t>(data.wire->stackSize, 4 * 1024 * 1024);

    composeCheckGfxThread(data);

    // Make sure that windows are UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, GraphicsContext::VariableName) == 0) {
        throw SHException("GFX.MainWindow must be unique, found another use!");
      }
    }

    _exposedVariables = ExposedInfo(data.shared);
    _exposedVariables.push_back(GraphicsContext::VariableInfo);
    _exposedVariables.push_back(GraphicsRendererContext::VariableInfo);
    _exposedVariables.push_back(InputContext::VariableInfo);
    data.shared = SHExposedTypesInfo(_exposedVariables);

    _shards.compose(data);

    return CoreInfo::NoneType;
  }

  void warmup(SHContext *context) {
    WindowCreationOptions windowOptions = {};
    windowOptions.width = _pwidth;
    windowOptions.height = _pheight;
    windowOptions.title = _title;
    _graphicsContext.window = std::make_shared<Window>();
    _graphicsContext.window->init(windowOptions);

    ContextCreationOptions contextOptions = {};
    contextOptions.debug = _debug;
    _graphicsContext.context = std::make_shared<Context>();
    _graphicsContext.context->init(*_graphicsContext.window.get(), contextOptions);

#if GFX_APPLE
    auto &mainOutput = _graphicsContext.context->getMetalViewContainer();
    auto &dispatcher = context->main->dispatcher;
    dispatcher.trigger(std::ref(mainOutput));
#endif

    _graphicsContext.renderer = std::make_shared<Renderer>(*_graphicsContext.context.get());
    _graphicsContext.renderer->setIgnoreCompilationErrors(_ignoreCompilationErrors);
    _graphicsRendererContext.renderer = _graphicsContext.renderer.get();

    _graphicsContext.shDrawQueue = SHDrawQueue{
        .queue = std::make_shared<DrawQueue>(),
    };

    _graphicsContextVar = referenceVariable(context, GraphicsContext::VariableName);
    assignVariableValue(*_graphicsContextVar, Var::Object(&_graphicsContext, GraphicsContext::Type));

    _graphicsRendererContextVar = referenceVariable(context, GraphicsRendererContext::VariableName);
    assignVariableValue(*_graphicsRendererContextVar, Var::Object(&_graphicsRendererContext, GraphicsRendererContext::Type));

    _inputContextVar = referenceVariable(context, InputContext::VariableName);
    assignVariableValue(*_inputContextVar, Var::Object(&_inputContext, InputContext::Type));

    _shards.warmup(context);
  }

  void cleanup() {
    _shards.cleanup();
    _graphicsContext = GraphicsContext{};
    _graphicsRendererContext = GraphicsRendererContext{};
    _inputContext = InputContext{};

    if (_graphicsContextVar) {
      if (_graphicsContextVar->refcount > 1) {
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to {}", _graphicsContextVar->refcount - 1,
                    GraphicsContext::VariableName);
      }
      releaseVariable(_graphicsContextVar);
    }

    if (_graphicsRendererContextVar) {
      if (_graphicsRendererContextVar->refcount > 1) {
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to {}", _graphicsRendererContextVar->refcount - 1,
                    GraphicsRendererContext::VariableName);
      }
      releaseVariable(_graphicsRendererContextVar);
    }

    if (_inputContextVar) {
      if (_inputContextVar->refcount > 1) {
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to {}", _inputContextVar->refcount - 1,
                    InputContext::VariableName);
      }
      releaseVariable(_inputContextVar);
    }
  }

  void frameBegan() { _graphicsContext.getDrawQueue()->clear(); }

  void updateVirtualInputEvents() {
    auto &virtualInputEvents = _inputContext.virtualInputEvents;
    auto &heldKeys = _inputContext.heldKeys;
    virtualInputEvents.clear();

    for (auto &evt : _inputContext.events) {
      switch (evt.type) {
      case SDL_KEYDOWN:
        if (heldKeys.insert(evt.key.keysym.sym).second) {
          virtualInputEvents.push_back(evt);
        }
        break;
      case SDL_KEYUP:
        if (heldKeys.contains(evt.key.keysym.sym)) {
          heldKeys.erase(evt.key.keysym.sym);
          virtualInputEvents.push_back(evt);
        }
        break;
      }
    }

    int numKeys{};
    auto keyStates = SDL_GetKeyboardState(&numKeys);
    _actuallyHeldKeys.clear();
    for (size_t i = 0; i < numKeys; i++) {
      SDL_Keycode code = SDL_GetKeyFromScancode((SDL_Scancode)i);
      if (keyStates[i] == 1) {
        _actuallyHeldKeys.insert(code);
      }
    }

    // Synthesize release events in case the event got lost
    for (auto it = heldKeys.begin(); it != heldKeys.end();) {
      if (!_actuallyHeldKeys.contains(*it)) {
        it = heldKeys.erase(it);
        // Generate virtual event
        SDL_Event evt{
            .key =
                {
                    .type = SDL_KEYUP,
                    .state = SDL_RELEASED,
                    .keysym =
                        {
                            .sym = *it,
                        },
                },
        };
        virtualInputEvents.push_back(evt);
      } else {
        ++it;
      }
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &renderer = _graphicsRendererContext.renderer;
    auto &context = _graphicsContext.context;
    auto &window = _graphicsContext.window;
    auto &events = _inputContext.events;

    window->pollEvents(events);
    for (auto &event : events) {
      if (event.type == SDL_QUIT) {
        throw ActivationError("Window closed, aborting wire.");
      } else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
          throw ActivationError("Window closed, aborting wire.");
        } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          gfx::int2 newSize = window->getDrawableSize();
          context->resizeMainOutputConditional(newSize);
        }
      } else if (event.type == SDL_APP_DIDENTERBACKGROUND) {
        context->suspend();
      } else if (event.type == SDL_APP_DIDENTERFOREGROUND) {
        context->resume();
      }
    }

    // Push root input region
    auto &inputStack = _inputContext.inputStack;
    inputStack.reset();
    inputStack.push(input::InputStack::Item{
        .windowMapping = input::WindowSubRegion::fromEntireWindow(*window.get()),
    });

    float deltaTime = 0.0;
    if (_loop.beginFrame(0.0f, deltaTime)) {
      if (context->beginFrame()) {
        _graphicsContext.time = _loop.getAbsoluteTime();
        _graphicsContext.deltaTime = deltaTime;

        renderer->beginFrame();

        frameBegan();

        SHVar _shardsOutput{};
        _shards.activate(shContext, input, _shardsOutput);

        renderer->endFrame();

        context->endFrame();
      }
    }

    updateVirtualInputEvents();

    inputStack.pop();

    return SHVar{};
  }
};

struct WindowSize {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_IMPL(WindowSize);

  RequiredGraphicsContext _requiredGraphicsContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredGraphicsContext.warmup(context);
  }
  void cleanup() {
    PARAM_CLEANUP();
    _requiredGraphicsContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return toVar(_requiredGraphicsContext->window->getSize()); }
};

struct ResizeWindow {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_IMPL(WindowSize);

  RequiredGraphicsContext _requiredGraphicsContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredGraphicsContext.warmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _requiredGraphicsContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _requiredGraphicsContext->window->resize(toInt2(input));
    return input;
  }
};
struct WindowPosition {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_IMPL(WindowPosition);

  RequiredGraphicsContext _requiredGraphicsContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredGraphicsContext.warmup(context);
  }
  void cleanup() {
    PARAM_CLEANUP();
    _requiredGraphicsContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return toVar(_requiredGraphicsContext->window->getPosition()); }
};

struct MoveWindow {
  static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  PARAM_IMPL(WindowSize);

  RequiredGraphicsContext _requiredGraphicsContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredGraphicsContext.warmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _requiredGraphicsContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _requiredGraphicsContext->window->move(toInt2(input));
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
