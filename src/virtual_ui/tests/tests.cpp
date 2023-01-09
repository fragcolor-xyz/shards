#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <spdlog/spdlog.h>
#include <linalg.h>
#include <stdint.h>
#include <gfx/tests/context.hpp>
#include <gfx/tests/renderer.hpp>
#include <gfx/tests/data.hpp>
#include <gfx/tests/renderer_utils.hpp>
#include <gfx/egui/egui_render_pass.hpp>
#include <gfx/egui/input.hpp>
#include <gfx/drawable.hpp>
#include <gfx/renderer.hpp>
#include <input/input.hpp>

using namespace gfx;

const float comparisonTolerance = 2.0f / 255.0f;

extern "C" {
struct VUIArgs {
  void *instance;
  DrawQueuePtr *queue;
  egui::Input *input;
  float drawScale = 1.0f;
  double time{};
  float deltaTime{};
};
void *vui_new_instance();
void vui_0(VUIArgs &args);
void vui_1(VUIArgs &args);
}

TEST_CASE("Virtual UI") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  ViewPtr view = createTestProjectionView();
  DrawQueuePtr queue = std::make_shared<DrawQueue>();

  auto frameSize = int2(200, 200);

  egui::Input eguiInput{};
  eguiInput.pixelsPerPoint = 2.0f;
  eguiInput.screenRect.max = egui::Pos2{.x = float(frameSize.x), .y = float(frameSize.y)};

  PipelineSteps steps = PipelineSteps{
      EguiRenderPass::createPipelineStep(queue),
  };

  auto uiArgs = VUIArgs{
      .instance = vui_new_instance(),
      .queue = &queue,
      .input = &eguiInput,
      .drawScale = eguiInput.pixelsPerPoint,
  };

  TEST_RENDER_LOOP(testRenderer) {
    queue->clear();
    eguiInput.screenRect.min = egui::Pos2{.x = 0, .y = 0};
    eguiInput.screenRect.max = egui::Pos2{.x = float(frameSize.x), .y = float(frameSize.y)};
    vui_0(uiArgs);
    eguiInput.screenRect.min = egui::Pos2{.x = 200, .y = 100};
    eguiInput.screenRect.max = egui::Pos2{.x = 200 + float(frameSize.x), .y = 100 + float(frameSize.y)};
    vui_1(uiArgs);
    renderer.render(view, steps);
  };
  CHECK(testRenderer->checkFrame("virtualUI", comparisonTolerance));
}

int main(int argc, char *argv[]) {
  Catch::Session session;

  spdlog::set_level(spdlog::level::debug);

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0) // Indicates a command line error
    return returnCode;

  auto &configData = session.configData();
  (void)configData;

  int result = session.run();

  return result;
}
