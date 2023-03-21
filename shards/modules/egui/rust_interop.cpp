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
  static SHTypeInfo type = InputContext::Type;
  return &type;
}
const char *gfx_getInputContextVarName() { return InputContext::VariableName; }

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

gfx::int4 gfx_getEguiMappedRegion(const SHVar &windowContextVar) {
  WindowContext &windowContext = varAsObjectChecked<WindowContext>(windowContextVar, WindowContext::Type);
  (void)windowContext;

  // TODO? input
  // auto &inputStack = windowContext.inputStack;
  // InputStack::Item inputStackOutput = inputStack.getTop();

  int4 result{};
  // if (inputStackOutput.windowMapping) {
  // }
  return result;
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
  InputContext &inputContext = varAsObjectChecked<InputContext>(inputContextVar, InputContext::Type);
  auto &window = inputContext.window;

  // static std::vector<Event> noEvents{};
  // const std::vector<Event> *eventsPtr = &noEvents;
  // int4 mappedWindowRegion{};

  int2 viewportSize{};
  if (graphicsContextVar) {
    GraphicsContext &graphicsContext = varAsObjectChecked<GraphicsContext>(*graphicsContextVar, GraphicsContext::Type);

    auto &viewStack = graphicsContext.renderer->getViewStack();
    auto viewStackOutput = viewStack.getOutput();

    // Get viewport size from view stack
    viewportSize = viewStackOutput.viewport.getSize();
  } else {
    viewportSize = window->getDrawableSize();
  }

  // Get events based on input stack
  // TODO: Input
  // auto &inputStack = windowContext.inputStack;
  // InputStack::Item inputStackOutput = inputStack.getTop();
  // if (inputStackOutput.windowMapping) {
  //   auto &windowMapping = inputStackOutput.windowMapping.value();
  //   std::visit(
  //       [&](auto &&arg) {
  //         using T = std::decay_t<decltype(arg)>;
  //         if constexpr (std::is_same_v<T, WindowSubRegion>) {
  //           mappedWindowRegion = arg.region;
  // eventsPtr = &inputContext.detached.virtualInputEvents;
  //         }
  //       },
  //       windowMapping);
  // }

  return translator->translateFromInputEvents(EguiInputTranslatorArgs{
      .events = inputContext.detached.virtualInputEvents,
      // .window = *window.get(),
      .time = inputContext.time,
      .deltaTime = inputContext.deltaTime,
      .region = inputContext.getState().region,
      // .viewportSize = viewportSize,
      // .mappedWindowRegion = mappedWindowRegion,
      // .scalingFactor = scalingFactor,
  });
}

void gfx_applyEguiOutputs(gfx::EguiInputTranslator *translator, const egui::FullOutput &output, const SHVar &inputContextVar) {
  InputContext &inputContext = varAsObjectChecked<InputContext>(inputContextVar, InputContext::Type);

  translator->applyOutput(output);
  for (auto &msg : translator->getOutputMessages()) {
    inputContext.master->postMessage(msg);
  }

  // Update these
  if (inputContext.wantsPointerInput != output.wantsPointerInput)
    SPDLOG_INFO("EGUI: wantsPointerInput = {}", output.wantsPointerInput);
  inputContext.wantsPointerInput = output.wantsPointerInput;

  if (inputContext.wantsKeyboardInput != output.wantsKeyboardInput)
    SPDLOG_INFO("EGUI: wantsKeyboardInput = {}", output.wantsKeyboardInput);
  inputContext.wantsKeyboardInput = output.wantsKeyboardInput;
}
