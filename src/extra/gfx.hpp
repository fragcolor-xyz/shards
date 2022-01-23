#include "gfx/chainblocks_types.hpp"
#include <SDL_events.h>
#include <chainblocks.hpp>
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/drawable.hpp>

namespace gfx {
struct Context;
struct Window;
struct Renderer;

struct MainWindowGlobals {
  static constexpr uint32_t TypeId = 'mwnd';
  static inline chainblocks::Type Type{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = TypeId}}}};

  std::shared_ptr<Context> context;
  std::shared_ptr<Window> window;
  std::shared_ptr<Renderer> renderer;
  std::vector<SDL_Event> events;
  ::gfx::DrawQueue drawQueue;
};

struct Base {
  static inline const char *mainWindowGlobalsVarName = "GFX.MainWindow";
  static inline CBExposedTypeInfo mainWindowGlobalsInfo =
      chainblocks::ExposedInfo::Variable(mainWindowGlobalsVarName, CBCCSTR("The main window context."), MainWindowGlobals::Type);
  static inline chainblocks::ExposedInfo requiredInfo = chainblocks::ExposedInfo(mainWindowGlobalsInfo);

  CBVar *_mainWindowGlobalsVar{nullptr};

  MainWindowGlobals &getMainWindowGlobals();
  ::gfx::Context &getContext();
  ::gfx::Window &getWindow();
};

struct BaseConsumer : public Base {
  CBExposedTypesInfo requiredVariables() { return CBExposedTypesInfo(Base::requiredInfo); }

  // Required before _bgfxCtx can be used
  void baseConsumerWarmup(CBContext *context) {
    _mainWindowGlobalsVar = chainblocks::referenceVariable(context, Base::mainWindowGlobalsVarName);
    assert(_mainWindowGlobalsVar->valueType == CBType::Object);
  }

  // Required during cleanup if _warmup() was called
  void baseConsumerCleanup() {
    if (_mainWindowGlobalsVar) {
      chainblocks::releaseVariable(_mainWindowGlobalsVar);
      _mainWindowGlobalsVar = nullptr;
    }
  }

  CBTypeInfo composeCheckMainThread(const CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw chainblocks::ComposeError("GFX Blocks cannot be used on a worker thread (e.g. "
                                      "within an Await block)");
    }

    // Return None to trigger assertion during validation
    return chainblocks::CoreInfo::NoneType;
  }

  void warmup(CBContext *context) { baseConsumerWarmup(context); }
  void cleanup(CBContext *context) { baseConsumerCleanup(); }
  CBTypeInfo compose(const CBInstanceData &data) { return composeCheckMainThread(data); }
};
} // namespace gfx