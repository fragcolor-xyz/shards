#ifndef SH_EXTRA_GFX
#define SH_EXTRA_GFX

#include "gfx/shards_types.hpp"
#include <SDL_events.h>
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/drawable.hpp>
#include <gfx/fwd.hpp>
#include <shards.hpp>

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

  double time;
  float deltaTime;

  // Draw queue used when it's not manually specified
  SHDrawQueue shDrawQueue;

  ::gfx::DrawQueuePtr getDrawQueue() { return shDrawQueue.queue; }
};

struct ContextUserData {
  SHContext *shardsContext{};
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
  void baseConsumerWarmup(SHContext *context, bool mainWindowRequired = true) {
    _mainWindowGlobalsVar = shards::referenceVariable(context, Base::mainWindowGlobalsVarName);
    if (mainWindowRequired) {
      assert(_mainWindowGlobalsVar->valueType == SHType::Object);
    }
  }

  // Required during cleanup if _warmup() was called
  void baseConsumerCleanup() {
    if (_mainWindowGlobalsVar) {
      shards::releaseVariable(_mainWindowGlobalsVar);
      _mainWindowGlobalsVar = nullptr;
    }
  }

  void composeCheckMainThread(const SHInstanceData &data) {
    if (data.onWorkerThread) {
      throw shards::ComposeError("GFX Shards cannot be used on a worker thread (e.g. "
                                 "within an Await shard)");
    }
  }

  void composeCheckMainWindowGlobals(const SHInstanceData &data) {
    bool variableFound = false;
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, Base::mainWindowGlobalsVarName) == 0) {
        variableFound = true;
      }
    }

    if (!variableFound)
      throw SHComposeError("MainWindow required, but not found");
  }

  void warmup(SHContext *context) { baseConsumerWarmup(context); }

  void cleanup(SHContext *context) { baseConsumerCleanup(); }

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckMainThread(data);
    composeCheckMainWindowGlobals(data);
    return shards::CoreInfo::NoneType;
  }
};
} // namespace gfx

#endif // SH_EXTRA_GFX
