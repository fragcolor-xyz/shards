#include "rust_interop.hpp"
#include "gfx/shards_types.hpp"
#include "gfx.hpp"
#include <gfx/renderer.cpp>
#include <foundation.hpp>

using namespace shards::input;
using shards::Var;
using namespace gfx;
using GFXTypes = gfx::Types;

SHTypeInfo *gfx_getMainWindowGlobalsType() {
  static SHTypeInfo type = gfx::MainWindowGlobals::Type;
  return &type;
}

const char *gfx_getMainWindowGlobalsVarName() { return gfx::Base::mainWindowGlobalsVarName; }

SHTypeInfo *gfx_getQueueType() {
  static SHTypeInfo type = GFXTypes::DrawQueue;
  return &type;
}

SHVar gfx_MainWindowGlobals_getDefaultQueue(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);
  return Var::Object(&globals->shDrawQueue, SHTypeInfo(GFXTypes::DrawQueue).object.vendorId,
                     SHTypeInfo(GFXTypes::DrawQueue).object.typeId);
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
  SHDrawQueue *shDrawQueue = varAsObjectChecked<SHDrawQueue>(var, GFXTypes::DrawQueue);
  return &shDrawQueue->queue;
}

gfx::int4 gfx_getEguiMappedRegion(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);

  auto &inputStack = globals->inputStack;
  InputStack::Item inputStackOutput = inputStack.getTop();

  int4 result{};
  if (inputStackOutput.windowMapping) {
  }
  return result;
}

gfx::int4 gfx_getViewport(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);

  auto &viewStack = globals->renderer->getViewStack();
  auto viewStackOutput = viewStack.getOutput();

  gfx::Rect viewportRect = viewStackOutput.viewport;
  return int4(viewportRect.x, viewportRect.y, viewportRect.getX1(), viewportRect.getY1());
}

const egui::Input *gfx_getEguiWindowInputs(gfx::EguiInputTranslator *translator, const SHVar &mainWindowGlobals,
                                           float scalingFactor) {
  MainWindowGlobals *globals = varAsObjectChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);

  static std::vector<SDL_Event> noEvents{};
  const std::vector<SDL_Event> *eventsPtr = &noEvents;
  int4 mappedWindowRegion;

  auto &viewStack = globals->renderer->getViewStack();
  auto viewStackOutput = viewStack.getOutput();

  // Get viewport size from view stack
  int2 viewportSize = viewStackOutput.viewport.getSize();

  // Get events based on input stack
  auto &inputStack = globals->inputStack;
  InputStack::Item inputStackOutput = inputStack.getTop();
  if (inputStackOutput.windowMapping) {
    auto &windowMapping = inputStackOutput.windowMapping.value();
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, WindowSubRegion>) {
            mappedWindowRegion = arg.region;
            eventsPtr = &globals->events;
          }
        },
        windowMapping);
  }

  return translator->translateFromInputEvents(EguiInputTranslatorArgs{
      .events = *eventsPtr,
      .window = *globals->window.get(),
      .time = globals->time,
      .deltaTime = globals->deltaTime,
      .viewportSize = viewportSize,
      .mappedWindowRegion = mappedWindowRegion,
      .scalingFactor = scalingFactor,
  });
}
