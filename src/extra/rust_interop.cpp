#include "rust_interop.hpp"
#include "gfx/shards_types.hpp"
#include "gfx.hpp"
#include "inputs.hpp"
#include <gfx/renderer.hpp>
#include <foundation.hpp>

using namespace shards::input;
using namespace gfx;
using namespace shards;
using GFXTypes = gfx::Types;

SHTypeInfo *gfx_getGraphicsContextType() {
  static SHTypeInfo type = gfx::GraphicsContext::Type;
  return &type;
}
const char *gfx_getGraphicsContextVarName() { return gfx::GraphicsContext::VariableName; }

SHTypeInfo *gfx_getInputContextType() {
  static SHTypeInfo type = Inputs::InputContext::Type;
  return &type;
}
const char *gfx_getInputContextVarName() { return Inputs::InputContext::VariableName; }

SHTypeInfo *gfx_getQueueType() {
  static SHTypeInfo type = GFXTypes::DrawQueue;
  return &type;
}

SHVar gfx_GraphicsContext_getDefaultQueue(const SHVar &graphicsContext) {
  GraphicsContext *globals = varAsObjectChecked<GraphicsContext>(graphicsContext, GraphicsContext::Type);
  return Var::Object(&globals->shDrawQueue, SHTypeInfo(GFXTypes::DrawQueue).object.vendorId,
                     SHTypeInfo(GFXTypes::DrawQueue).object.typeId);
}
Context *gfx_GraphicsContext_getContext(const SHVar &graphicsContext) {
  GraphicsContext *globals = varAsObjectChecked<GraphicsContext>(graphicsContext, GraphicsContext::Type);
  return globals->context.get();
}
Renderer *gfx_GraphicsContext_getRenderer(const SHVar &graphicsContext) {
  GraphicsContext *globals = varAsObjectChecked<GraphicsContext>(graphicsContext, GraphicsContext::Type);
  return globals->renderer.get();
}

DrawQueuePtr *gfx_getDrawQueueFromVar(const SHVar &var) {
  SHDrawQueue *shDrawQueue = varAsObjectChecked<SHDrawQueue>(var, GFXTypes::DrawQueue);
  return &shDrawQueue->queue;
}

gfx::int4 gfx_getEguiMappedRegion(const SHVar &inputContextVar) {
  Inputs::InputContext *inputContext = varAsObjectChecked<Inputs::InputContext>(inputContextVar, Inputs::InputContext::Type);

  auto &inputStack = inputContext->inputStack;
  InputStack::Item inputStackOutput = inputStack.getTop();

  int4 result{};
  if (inputStackOutput.windowMapping) {
  }
  return result;
}

gfx::int4 gfx_getViewport(const SHVar &graphicsContextVar) {
  GraphicsContext *graphicsContext = varAsObjectChecked<GraphicsContext>(graphicsContextVar, GraphicsContext::Type);

  auto &viewStack = graphicsContext->renderer->getViewStack();
  auto viewStackOutput = viewStack.getOutput();

  gfx::Rect viewportRect = viewStackOutput.viewport;
  return int4(viewportRect.x, viewportRect.y, viewportRect.getX1(), viewportRect.getY1());
}

const egui::Input *gfx_getEguiWindowInputs(gfx::EguiInputTranslator *translator, const SHVar &graphicsContextVar,
                                           const SHVar &inputContextVar, float scalingFactor) {
  GraphicsContext *graphicsContext = varAsObjectChecked<GraphicsContext>(graphicsContextVar, GraphicsContext::Type);
  Inputs::InputContext *inputContext = varAsObjectChecked<Inputs::InputContext>(inputContextVar, Inputs::InputContext::Type);

  static std::vector<SDL_Event> noEvents{};
  const std::vector<SDL_Event> *eventsPtr = &noEvents;
  int4 mappedWindowRegion;

  auto &viewStack = graphicsContext->renderer->getViewStack();
  auto viewStackOutput = viewStack.getOutput();

  // Get viewport size from view stack
  int2 viewportSize = viewStackOutput.viewport.getSize();

  // Get events based on input stack
  auto &inputStack = inputContext->inputStack;
  InputStack::Item inputStackOutput = inputStack.getTop();
  if (inputStackOutput.windowMapping) {
    auto &windowMapping = inputStackOutput.windowMapping.value();
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, WindowSubRegion>) {
            mappedWindowRegion = arg.region;
            eventsPtr = &inputContext->events;
          }
        },
        windowMapping);
  }

  return translator->translateFromInputEvents(EguiInputTranslatorArgs{
      .events = *eventsPtr,
      .window = *graphicsContext->window.get(),
      .time = graphicsContext->time,
      .deltaTime = graphicsContext->deltaTime,
      .viewportSize = viewportSize,
      .mappedWindowRegion = mappedWindowRegion,
      .scalingFactor = scalingFactor,
  });
}
