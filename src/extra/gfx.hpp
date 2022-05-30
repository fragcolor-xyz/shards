#ifndef SH_EXTRA_GFX
#define SH_EXTRA_GFX

#include "gfx/shards_types.hpp"
#include <SDL_events.h>
#include <shards.hpp>
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/drawable.hpp>
#include <gfx/fwd.hpp>

namespace gfx {
struct ImGuiRenderer;
struct Window;
struct Renderer;

struct MainWindowGlobals {
  static constexpr uint32_t TypeId = 'mwnd';
  static inline shards::Type Type{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = TypeId}}}};

  std::shared_ptr<Context> context;
  std::shared_ptr<Window> window;
  std::shared_ptr<Renderer> renderer;
  std::shared_ptr<ImGuiRenderer> imgui;
  std::vector<SDL_Event> events;
  ::gfx::DrawQueue drawQueue;
};

struct Base {
  static inline const char *mainWindowGlobalsVarName = "GFX.MainWindow";
  static inline SHExposedTypeInfo mainWindowGlobalsInfo =
      shards::ExposedInfo::Variable(mainWindowGlobalsVarName, SHCCSTR("The main window context."), MainWindowGlobals::Type);
  static inline shards::ExposedInfo requiredInfo = shards::ExposedInfo(mainWindowGlobalsInfo);

  SHVar *_mainWindowGlobalsVar{nullptr};

  MainWindowGlobals &getMainWindowGlobals();
  ::gfx::Context &getContext();
  ::gfx::Window &getWindow();
  SDL_Window *getSdlWindow();
};

struct BaseConsumer : public Base {
  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(Base::requiredInfo); }

  // Required before _bgfxCtx can be used
  void baseConsumerWarmup(SHContext *context) {
    _mainWindowGlobalsVar = shards::referenceVariable(context, Base::mainWindowGlobalsVarName);
    assert(_mainWindowGlobalsVar->valueType == SHType::Object);
  }

  // Required during cleanup if _warmup() was called
  void baseConsumerCleanup() {
    if (_mainWindowGlobalsVar) {
      shards::releaseVariable(_mainWindowGlobalsVar);
      _mainWindowGlobalsVar = nullptr;
    }
  }

  SHTypeInfo composeCheckMainThread(const SHInstanceData &data) {
    if (data.onWorkerThread) {
      throw shards::ComposeError("GFX Shards cannot be used on a worker thread (e.g. "
                                      "within an Await shard)");
    }

    // Return None to trigger assertion during validation
    return shards::CoreInfo::NoneType;
  }

  void warmup(SHContext *context) { baseConsumerWarmup(context); }
  void cleanup(SHContext *context) { baseConsumerCleanup(); }
  SHTypeInfo compose(const SHInstanceData &data) { return composeCheckMainThread(data); }
};
} // namespace gfx

#endif // SH_EXTRA_GFX
