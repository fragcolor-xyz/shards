#include "rust_interop.hpp"
#include "gfx/shards_types.hpp"
#include "gfx.hpp"
#include <foundation.hpp>

using shards::Var;
using namespace gfx;

SHTypeInfo *gfx_getMainWindowGlobalsType() {
  static SHTypeInfo type = gfx::MainWindowGlobals::Type;
  return &type;
}

const char *gfx_getMainWindowGlobalsVarName() { return gfx::Base::mainWindowGlobalsVarName; }

SHTypeInfo *gfx_getQueueType() {
  static SHTypeInfo type = Types::DrawQueue;
  return &type;
}

SHVar gfx_MainWindowGlobals_getDefaultQueue(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);
  return Var::Object(&globals->shDrawQueue, SHTypeInfo(Types::DrawQueue).object.vendorId,
                     SHTypeInfo(Types::DrawQueue).object.typeId);
}
Context *gfx_MainWindowGlobals_getContext(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);
  return globals->context.get();
}
Renderer *gfx_MainWindowGlobals_getRenderer(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);
  return globals->renderer.get();
}

DrawQueuePtr *gfx_getDrawQueueFromVar(const SHVar &var) {
  SHDrawQueue *shDrawQueue = varAsObjectChecked<SHDrawQueue>(var, Types::DrawQueue);
  return &shDrawQueue->queue;
}

gfx::int4 gfx_getEguiMappedRegion(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);

  auto &viewStack = globals->renderer->getViewStack();
  ViewStack::Output viewStackOutput = viewStack.getOutput();

  int4 result{};
  if (viewStackOutput.windowMapping) {
    auto &windowMapping = viewStackOutput.windowMapping.value();
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, WindowSubRegion>) {
            Rect &region = arg.region;
            result = int4(region.x, region.y, region.getX1(), region.getY1());
          }
        },
        windowMapping);
  }
  return result;
}

const egui::Input *gfx_getEguiWindowInputs(gfx::EguiInputTranslator *translator, const SHVar &mainWindowGlobals,
                                           float scalingFactor) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);

  auto &viewStack = globals->renderer->getViewStack();
  ViewStack::Output viewStackOutput = viewStack.getOutput();

  static std::vector<SDL_Event> noEvents{};
  const std::vector<SDL_Event> *eventsPtr = &noEvents;
  if (viewStackOutput.windowMapping) {
    eventsPtr = &globals->events;
  }

  return translator->translateFromInputEvents(*eventsPtr, *globals->window.get(), globals->time, globals->deltaTime,
                                              scalingFactor);
}
