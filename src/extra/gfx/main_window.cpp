#include "../gfx.hpp"
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>

using namespace chainblocks;
namespace gfx {

MainWindowGlobals &Base::getMainWindowGlobals() {
  MainWindowGlobals *globalsPtr = reinterpret_cast<MainWindowGlobals *>(_mainWindowGlobalsVar->payload.objectValue);
  if (!globalsPtr) {
    throw chainblocks::ActivationError("Graphics context not set");
  }
  return *globalsPtr;
}
Context &Base::getContext() { return *getMainWindowGlobals().context.get(); }
Window &Base::getWindow() { return *getMainWindowGlobals().window.get(); }

struct MainWindow : public Base {
  static inline Parameters params{
      {"Title", CBCCSTR("The title of the window to create."), {CoreInfo::StringType}},
      {"Width", CBCCSTR("The width of the window to create. In pixels and DPI aware."), {CoreInfo::IntType}},
      {"Height", CBCCSTR("The height of the window to create. In pixels and DPI aware."), {CoreInfo::IntType}},
      {"Contents", CBCCSTR("The contents of this window."), {CoreInfo::BlocksOrNone}},
      {"Debug",
       CBCCSTR("If the device backing the window should be created with "
               "debug layers on."),
       {CoreInfo::BoolType}},
  };

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBParametersInfo parameters() { return params; }

  std::string _title;
  int _pwidth = 1280;
  int _pheight = 720;
  bool _debug = false;
  Window _window;
  BlocksVar _blocks;

  std::shared_ptr<MainWindowGlobals> globals;
  ::gfx::Loop loop;

  void setParam(int index, const CBVar &value) {
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
      _blocks = value;
      break;
    case 4:
      _debug = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_title);
    case 1:
      return Var(_pwidth);
    case 2:
      return Var(_pheight);
    case 3:
      return _blocks;
    case 4:
      return Var(_debug);
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("GFX Blocks cannot be used on a worker thread (e.g. "
                         "within an Await block)");
    }

    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
        throw CBException("GFX.MainWindow must be unique, found another use!");
      }
    }

    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
        throw CBException("GFX.MainWindow must be unique, found another use!");
      }
    }

    globals = std::make_shared<MainWindowGlobals>();

    // twice to actually own the data and release...
    IterableExposedInfo rshared(data.shared);
    IterableExposedInfo shared(rshared);
    shared.push_back(
        ExposedInfo::ProtectedVariable(Base::mainWindowGlobalsVarName, CBCCSTR("The graphics context"), MainWindowGlobals::Type));
    data.shared = shared;

    _blocks.compose(data);

    return CoreInfo::NoneType;
  }

  void warmup(CBContext *context) {
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

    _mainWindowGlobalsVar = referenceVariable(context, Base::mainWindowGlobalsVarName);
    _mainWindowGlobalsVar->payload.objectTypeId = MainWindowGlobals::TypeId;
    _mainWindowGlobalsVar->payload.objectValue = globals.get();
    _mainWindowGlobalsVar->valueType = CBType::Object;

    _blocks.warmup(context);
  }

  void cleanup() {
    globals.reset();
    _blocks.cleanup();

    if (_mainWindowGlobalsVar) {
      if (_mainWindowGlobalsVar->refcount > 1) {
#ifdef NDEBUG
        CBLOG_ERROR("MainWindow: Found a dangling reference to GFX.Context");
#else
        CBLOG_ERROR("MainWindow: Found {} dangling reference(s) to GFX.Context", _mainWindowGlobalsVar->refcount - 1);
#endif
      }
      memset(_mainWindowGlobalsVar, 0x0, sizeof(CBVar));
      _mainWindowGlobalsVar = nullptr;
    }
  }

  CBVar activate(CBContext *cbContext, const CBVar &input) {
    auto &renderer = globals->renderer;
    auto &context = globals->context;
    auto &window = globals->window;
    auto &events = globals->events;

    window->pollEvents(events);
    for (auto &event : events) {
      if (event.type == SDL_QUIT) {
        throw ActivationError("Window closed, aborting chain.");
      } else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
          throw ActivationError("Window closed, aborting chain.");
        } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          gfx::int2 newSize = window->getDrawableSize();
          context->resizeMainOutputConditional(newSize);
        }
      }
    }

    float deltaTime = 0.0;
    auto &drawQueue = globals->drawQueue;
    if (loop.beginFrame(0.0f, deltaTime)) {
      context->beginFrame();
      renderer->swapBuffers();
      drawQueue.clear();

      CBVar _blocksOutput{};
      _blocks.activate(cbContext, input, _blocksOutput);

      context->endFrame();
    }

    return CBVar{};
  }
};

void registerMainWindowBlocks() { REGISTER_CBLOCK("GFX.MainWindow", MainWindow); }

} // namespace gfx
