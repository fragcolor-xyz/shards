#include "../gfx.hpp"
#include <gfx/context.hpp>
#include <gfx/imgui/imgui.hpp>
#include <gfx/loop.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>

using namespace shards;
namespace gfx {

MainWindowGlobals &Base::getMainWindowGlobals() {
  MainWindowGlobals *globalsPtr = reinterpret_cast<MainWindowGlobals *>(_mainWindowGlobalsVar->payload.objectValue);
  if (!globalsPtr) {
    throw shards::ActivationError("Graphics context not set");
  }
  return *globalsPtr;
}
Context &Base::getContext() { return *getMainWindowGlobals().context.get(); }
Window &Base::getWindow() { return *getMainWindowGlobals().window.get(); }
SDL_Window *Base::getSdlWindow() { return getWindow().window; }

struct MainWindow : public Base {
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

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHParametersInfo parameters() { return params; }

  std::string _title;
  int _pwidth = 1280;
  int _pheight = 720;
  bool _debug = false;
  Window _window;
  ShardsVar _shards;

  std::shared_ptr<MainWindowGlobals> globals;
  ::gfx::Loop loop;

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
    if (data.onWorkerThread) {
      throw ComposeError("GFX Shards cannot be used on a worker thread (e.g. "
                         "within an Await shard)");
    }

    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
        throw SHException("GFX.MainWindow must be unique, found another use!");
      }
    }

    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
        throw SHException("GFX.MainWindow must be unique, found another use!");
      }
    }

    globals = std::make_shared<MainWindowGlobals>();

    // twice to actually own the data and release...
    IterableExposedInfo rshared(data.shared);
    IterableExposedInfo shared(rshared);
    shared.push_back(
        ExposedInfo::ProtectedVariable(Base::mainWindowGlobalsVarName, SHCCSTR("The graphics context"), MainWindowGlobals::Type));
    data.shared = shared;

    _shards.compose(data);

    return CoreInfo::NoneType;
  }

  void warmup(SHContext *context) {
    globals = std::make_shared<MainWindowGlobals>();

    WindowCreationOptions windowOptions = {};
    windowOptions.width = _pwidth;
    windowOptions.height = _pheight;
    windowOptions.title = _title;
    globals->window = std::make_shared<Window>();
    globals->window->init(windowOptions);

    ContextCreationOptions contextOptions = {};
    contextOptions.debug = _debug;
    globals->context = std::make_shared<Context>();
    globals->context->init(*globals->window.get(), contextOptions);

    globals->renderer = std::make_shared<Renderer>(*globals->context.get());
    globals->imgui = std::make_shared<ImGuiRenderer>(*globals->context.get());

    _mainWindowGlobalsVar = referenceVariable(context, Base::mainWindowGlobalsVarName);
    _mainWindowGlobalsVar->payload.objectTypeId = MainWindowGlobals::TypeId;
    _mainWindowGlobalsVar->payload.objectValue = globals.get();
    _mainWindowGlobalsVar->valueType = SHType::Object;

    _shards.warmup(context);
  }

  void cleanup() {
    globals.reset();
    _shards.cleanup();

    if (_mainWindowGlobalsVar) {
      if (_mainWindowGlobalsVar->refcount > 1) {
#ifdef NDEBUG
        SHLOG_ERROR("MainWindow: Found a dangling reference to GFX.Context");
#else
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to GFX.Context", _mainWindowGlobalsVar->refcount - 1);
#endif
      }
      memset(_mainWindowGlobalsVar, 0x0, sizeof(SHVar));
      _mainWindowGlobalsVar = nullptr;
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &renderer = globals->renderer;
    auto &context = globals->context;
    auto &window = globals->window;
    auto &events = globals->events;
    auto &imgui = globals->imgui;

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

    float deltaTime = 0.0;
    auto &drawQueue = globals->drawQueue;
    if (loop.beginFrame(0.0f, deltaTime)) {
      if (context->beginFrame()) {
        imgui->beginFrame(events);
        renderer->beginFrame();

        drawQueue.clear();

        SHVar _shardsOutput{};
        _shards.activate(shContext, input, _shardsOutput);

        renderer->endFrame();
        imgui->endFrame();

        context->endFrame();
      }
    }

    return SHVar{};
  }
};

void registerMainWindowShards() { REGISTER_SHARD("GFX.MainWindow", MainWindow); }

} // namespace gfx
