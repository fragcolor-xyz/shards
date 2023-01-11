#include "../gfx.hpp"
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>
#include "../inputs.hpp"

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

  std::shared_ptr<InputContext> _inputContext;
  SHVar *_inputContextVar{};

  std::shared_ptr<GraphicsContext> _graphicsContext;
  SHVar *_graphicsContextVar{};

  std::shared_ptr<ContextUserData> _contextUserData;
  ::gfx::Loop _loop;

  ExposedInfo _exposedVariables;

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
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckGfxThread(data);

    // Make sure that windows are UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, GraphicsContext::VariableName) == 0) {
        throw SHException("GFX.MainWindow must be unique, found another use!");
      }
    }

    _graphicsContext = std::make_shared<GraphicsContext>();
    _inputContext = std::make_shared<InputContext>();

    _exposedVariables = ExposedInfo(data.shared);
    _exposedVariables.push_back(GraphicsContext::VariableInfo);
    _exposedVariables.push_back(InputContext::VariableInfo);
    data.shared = SHExposedTypesInfo(_exposedVariables);

    _shards.compose(data);

    return CoreInfo::NoneType;
  }

  void warmup(SHContext *context) {
    _graphicsContext = std::make_shared<GraphicsContext>();
    _contextUserData = std::make_shared<ContextUserData>();

    WindowCreationOptions windowOptions = {};
    windowOptions.width = _pwidth;
    windowOptions.height = _pheight;
    windowOptions.title = _title;
    _graphicsContext->window = std::make_shared<Window>();
    _graphicsContext->window->init(windowOptions);

    ContextCreationOptions contextOptions = {};
    contextOptions.debug = _debug;
    _graphicsContext->context = std::make_shared<Context>();
    _graphicsContext->context->init(*_graphicsContext->window.get(), contextOptions);

    // Attach user data to context
    _graphicsContext->context->userData.set(_contextUserData.get());

    _graphicsContext->renderer = std::make_shared<Renderer>(*_graphicsContext->context.get());

    _graphicsContext->shDrawQueue = SHDrawQueue{
        .queue = std::make_shared<DrawQueue>(),
    };

    _graphicsContextVar = referenceVariable(context, GraphicsContext::VariableName);
    _graphicsContextVar->payload.objectTypeId = SHTypeInfo(GraphicsContext::Type).object.typeId;
    _graphicsContextVar->payload.objectVendorId = SHTypeInfo(GraphicsContext::Type).object.vendorId;
    _graphicsContextVar->payload.objectValue = _graphicsContext.get();
    _graphicsContextVar->valueType = SHType::Object;

    _inputContextVar = referenceVariable(context, InputContext::VariableName);
    _inputContextVar->payload.objectTypeId = SHTypeInfo(InputContext::Type).object.typeId;
    _inputContextVar->payload.objectVendorId = SHTypeInfo(InputContext::Type).object.vendorId;
    _inputContextVar->payload.objectValue = _inputContext.get();
    _inputContextVar->valueType = SHType::Object;

    _shards.warmup(context);
  }

  void cleanup() {
    _graphicsContext.reset();
    _inputContext.reset();
    _shards.cleanup();

    if (_graphicsContextVar) {
      if (_graphicsContextVar->refcount > 1) {
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to {}", _graphicsContextVar->refcount - 1, GraphicsContext::VariableName);
      }
      releaseVariable(_graphicsContextVar);
    }

    if (_inputContextVar) {
      if (_inputContextVar->refcount > 1) {
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to {}", _inputContextVar->refcount - 1, InputContext::VariableName);
      }
      releaseVariable(_inputContextVar);
    }
  }

  void frameBegan() { _graphicsContext->getDrawQueue()->clear(); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &renderer = _graphicsContext->renderer;
    auto &context = _graphicsContext->context;
    auto &window = _graphicsContext->window;
    auto &events = _inputContext->events;

    // Store shards context for current activation in user data
    // this is used by callback chains that need to resolve variables, etc.
    _contextUserData->shardsContext = shContext;

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
    auto &inputStack = _inputContext->inputStack;
    inputStack.reset();
    inputStack.push(input::InputStack::Item{
        .windowMapping = input::WindowSubRegion::fromEntireWindow(*window.get()),
    });

    float deltaTime = 0.0;
    if (_loop.beginFrame(0.0f, deltaTime)) {
      if (context->beginFrame()) {
        _graphicsContext->time = _loop.getAbsoluteTime();
        _graphicsContext->deltaTime = deltaTime;

        renderer->beginFrame();

        frameBegan();

        SHVar _shardsOutput{};
        _shards.activate(shContext, input, _shardsOutput);

        renderer->endFrame();

        context->endFrame();
      }
    }

    inputStack.pop();

    // Clear context
    _contextUserData->shardsContext = nullptr;

    return SHVar{};
  }
};

void registerMainWindowShards() { REGISTER_SHARD("GFX.MainWindow", MainWindow); }

} // namespace gfx
