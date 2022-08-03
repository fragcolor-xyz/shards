#include <linalg.h>
#include "spdlog/spdlog.h"
#include <SDL_events.h>
#include <SDL_video.h>
#include <cassert>
#include <cstring>
#include <exception>
#include <gfx/context.hpp>
#include <gfx/drawable.hpp>
#include <gfx/enums.hpp>
#include <gfx/features/base_color.hpp>
#include <gfx/features/debug_color.hpp>
#include <gfx/features/transform.hpp>
#include <gfx/fmt.hpp>
#include <gfx/geom.hpp>
#include <gfx/linalg.hpp>
#include <gfx/loop.hpp>
#include <gfx/mesh.hpp>
#include <gfx/paths.hpp>
#include <gfx/renderer.hpp>
#include <gfx/types.hpp>
#include <gfx/utils.hpp>
#include <gfx/view.hpp>
#include <gfx/window.hpp>
#include <gfx/egui/egui_render_pass.hpp>
#include <gfx/egui/input.hpp>
#include <input/input.hpp>
#include <magic_enum.hpp>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace gfx;
using shards::input::ConsumeEventFilter;
using shards::input::InputBuffer;

#if GFX_EMSCRIPTEN
#include <emscripten/html5.h>
void osYield() {
  auto callback = [](double time, void *userData) -> EM_BOOL { return false; };
  emscripten_request_animation_frame(callback, nullptr);
  emscripten_sleep(0);
}
#else
void osYield() {}
#endif

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
void vui_drop_instance(void* inst);
void vui_0(VUIArgs &args);
void vui_1(VUIArgs &args);
}

struct App {
  Window window;
  Context context;

  Loop loop;
  float deltaTime;

  ViewPtr view;
  std::shared_ptr<Renderer> renderer;

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  PipelineSteps pipelineSteps;

  InputBuffer inputBuffer;

  App() {}

  void init(const char *hostElement) {
    window.init(WindowCreationOptions{.width = 1280, .height = 720});

    ContextCreationOptions contextOptions = {};
    contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
    context.init(window, contextOptions);

    renderer = std::make_shared<Renderer>(context);

    view = std::make_shared<View>();
    view->view = linalg::lookat_matrix(float3(3.0f, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
    view->proj = ViewPerspectiveProjection{
        degToRad(45.0f),
        FovDirection::Horizontal,
    };

    // auto glbPath = gfx::resolveDataPath(fmt::format("external/glTF-Sample-Models/2.0/{0}/glTF-Binary/{0}.glb", "Duck"));
    // duck = loadGltfFromFile(glbPath.string().c_str());

    pipelineSteps = {
        EguiRenderPass::createPipelineStep(queue),
    };
  }

  struct EguiInstance {
    egui::Input eguiInput0{};
    VUIArgs eguiArgs0;
    void (*rustFn)(VUIArgs &) = &vui_0;

    EguiInstance(float4 rect) {
      eguiInput0.pixelsPerPoint = 2.0f;
      eguiInput0.screenRect.min = egui::Pos2{.x = rect.x, .y = rect.y};
      eguiInput0.screenRect.max = egui::Pos2{.x = rect.z, .y = rect.w};

      eguiArgs0 = VUIArgs{
          .instance = vui_new_instance(),
          .input = &eguiInput0,
          .drawScale = eguiInput0.pixelsPerPoint,
      };
    }
    ~EguiInstance() {
      vui_drop_instance(eguiArgs0.instance);
    }
    EguiInstance(const EguiInstance &) = delete;
    EguiInstance &operator=(const EguiInstance &) = delete;
    void eval(DrawQueuePtr queue) {
      eguiArgs0.queue = &queue;
      rustFn(eguiArgs0);
    }
  };

  std::vector<std::shared_ptr<EguiInstance>> uiInstances;
  const float createNewUIDelay = 0.3f;
  float counter = createNewUIDelay;

  void renderFrame(double time, float deltaTime) {
    renderer->beginFrame();

    queue->clear();

    counter += deltaTime;
    if (counter >= createNewUIDelay) {
      if (uiInstances.size() > 64)
        uiInstances.clear();
      counter -= createNewUIDelay;
      float2 offset = float2(2.0f, 1.0f) * uiInstances.size();
      uiInstances.emplace_back(std::make_shared<EguiInstance>(float4(offset.x, offset.y, 200.0f + offset.x, 200.0f + offset.y)));
      SPDLOG_INFO("Now at {} egui instances", uiInstances.size());
    }

    for (auto &inst : uiInstances) {
      inst->eval(queue);
    }

    renderer->render(view, pipelineSteps);
    renderer->endFrame();
  }

  void runMainLoop() {
    bool quit = false;

    while (!quit) {
      if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {
        inputBuffer.clear();
        window.pollEventsForEach([&](auto evt) { inputBuffer.push_back(evt); });
        for (auto &event : inputBuffer) {
          if (event.type == SDL_QUIT)
            quit = true;
        }

        context.resizeMainOutputConditional(window.getDrawableSize());

        if (context.beginFrame()) {
          renderer->beginFrame();

          renderFrame(loop.getAbsoluteTime(), deltaTime);

          renderer->endFrame();
          context.endFrame();
        }
      }
      osYield();
    }
  }
};

extern "C" int main(int, char**) {
  App instance;
  instance.init(nullptr);
  instance.runMainLoop();
  return 0;
}
