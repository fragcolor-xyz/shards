#include "rust_interop.hpp"
#include <shards/modules/gfx/gfx.hpp>
#include <shards/modules/gfx/window.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include <gfx/renderer.hpp>
#include <shards/core/foundation.hpp>
#include <spdlog/spdlog.h>

using namespace shards::input;
using namespace gfx;
using namespace shards;
using GFXTypes = gfx::Types;

SHTypeInfo *gfx_getGraphicsContextType() {
  static SHTypeInfo type = gfx::GraphicsContext::Type;
  return &type;
}
const char *gfx_getGraphicsContextVarName() { return gfx::GraphicsContext::VariableName; }

SHTypeInfo *gfx_getWindowContextType() {
  static SHTypeInfo type = WindowContext::Type;
  return &type;
}
const char *gfx_getWindowContextVarName() { return WindowContext::VariableName; }

SHTypeInfo *gfx_getInputContextType() {
  static SHTypeInfo type = IInputContext::Type;
  return &type;
}
const char *gfx_getInputContextVarName() { return IInputContext::VariableName; }

SHTypeInfo *gfx_getQueueType() {
  static SHTypeInfo type = GFXTypes::DrawQueue;
  return &type;
}

gfx::Context *gfx_GraphicsContext_getContext(const SHVar &graphicsContext) {
  GraphicsContext &globals = varAsObjectChecked<GraphicsContext>(graphicsContext, GraphicsContext::Type);
  return globals.context.get();
}
gfx::Renderer *gfx_GraphicsContext_getRenderer(const SHVar &graphicsContext) {
  GraphicsContext &globals = varAsObjectChecked<GraphicsContext>(graphicsContext, GraphicsContext::Type);
  return globals.renderer.get();
}

DrawQueuePtr *gfx_getDrawQueueFromVar(const SHVar &var) {
  SHDrawQueue &shDrawQueue = varAsObjectChecked<SHDrawQueue>(var, GFXTypes::DrawQueue);
  return &shDrawQueue.queue;
}

gfx::int4 gfx_getViewport(const SHVar &graphicsContextVar) {
  GraphicsContext &graphicsContext = varAsObjectChecked<GraphicsContext>(graphicsContextVar, GraphicsContext::Type);

  auto &viewStack = graphicsContext.renderer->getViewStack();
  auto viewStackOutput = viewStack.getOutput();

  gfx::Rect viewportRect = viewStackOutput.viewport;
  return int4(viewportRect.x, viewportRect.y, viewportRect.getX1(), viewportRect.getY1());
}

const egui::Input *gfx_getEguiWindowInputs(gfx::EguiInputTranslator *translator, const SHVar *graphicsContextVar,
                                           const SHVar &inputContextVar, float scalingFactor) {
  IInputContext &inputContext = varAsObjectChecked<IInputContext>(inputContextVar, IInputContext::Type);

  static std::vector<Event> noEvents{};
  const std::vector<Event> *eventsPtr = &noEvents;
  int4 mappedWindowRegion{};

  InputRegion region = inputContext.getState().region;
  region.uiScalingFactor *= scalingFactor;

  // if (graphicsContextVar) {
  //   GraphicsContext &graphicsContext = varAsObjectChecked<GraphicsContext>(*graphicsContextVar, GraphicsContext::Type);

  //   auto &viewStack = graphicsContext.renderer->getViewStack();
  //   auto viewStackOutput = viewStack.getOutput();

  //   // Get viewport size from view stack
  //   region.pixelSize = (int2)viewStackOutput.viewport.getSize();
  //   region.size = (float2)viewStackOutput.viewport.getSize();
  // }

  // Get events based on input stack
  // TODO: Input
  auto &inputStack = inputContext.getInputStack();
  InputStack::Item inputStackOutput = inputStack.getTop();
  if (inputStackOutput.windowMapping) {
    auto &windowMapping = inputStackOutput.windowMapping.value();
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, WindowSubRegion>) {
            mappedWindowRegion = arg.region;
            eventsPtr = &inputContext.getEvents();
          }
        },
        windowMapping);
  }

  return translator->translateFromInputEvents(EguiInputTranslatorArgs{
      .events = *eventsPtr,
      .time = inputContext.getTime(),
      .deltaTime = inputContext.getDeltaTime(),
      .region = region,
      .mappedWindowRegion = mappedWindowRegion,
  });
}

void gfx_applyEguiOutputs(gfx::EguiInputTranslator *translator, const egui::FullOutput &output, const SHVar &inputContextVar) {
  IInputContext &inputContext = varAsObjectChecked<IInputContext>(inputContextVar, IInputContext::Type);

  translator->applyOutput(output);
  for (auto &msg : translator->getOutputMessages()) {
    inputContext.postMessage(msg);
  }

  // Update these
  auto &consumeFlags = inputContext.getConsumeFlags();
  // if (consumeFlags.wantsPointerInput != output.wantsPointerInput)
  //   SPDLOG_INFO("EGUI: wantsPointerInput = {}", output.wantsPointerInput);
  consumeFlags.wantsPointerInput = output.wantsPointerInput;

  // if (consumeFlags.wantsKeyboardInput != output.wantsKeyboardInput)
  //   SPDLOG_INFO("EGUI: wantsKeyboardInput = {}", output.wantsKeyboardInput);
  consumeFlags.wantsKeyboardInput = output.wantsKeyboardInput;

  // Request focus during drag operations
  if (output.wantsPointerInput && inputContext.getState().mouseButtonState != 0) {
    consumeFlags.requestFocus = true;
  } else {
    consumeFlags.requestFocus = false;
  }
}
