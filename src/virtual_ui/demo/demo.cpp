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
#include <gfx/gizmos/gizmos.hpp>
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
#include <virtual_ui/context.hpp>
#include <input/input.hpp>
#include <magic_enum.hpp>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace gfx;
using shards::input::ConsumeEventFilter;
using shards::input::InputBuffer;
using shards::vui::PanelRenderCallback;
using VUIContext = shards::vui::Context;
using VUIPanel = shards::vui::Panel;
using VUIPanelPtr = shards::vui::PanelPtr;

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
  void *renderContext;
  PanelRenderCallback render;
  const egui::Input *input;
};
void *vui_new_instance();
void vui_drop_instance(void *inst);
void vui_0(VUIArgs &args);
void vui_1(VUIArgs &args);
void vui_2(VUIArgs &args);
}

struct App {
  Window window;
  Context context;

  Loop loop;
  float deltaTime;

  ViewPtr view;
  std::shared_ptr<Renderer> renderer;

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  DrawQueuePtr uiQueue = std::make_shared<DrawQueue>();
  PipelineSteps pipelineSteps;

  InputBuffer inputBuffer;

  GizmoRenderer gr;

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
        makePipelineStep(RenderDrawablesStep{
            .drawQueue = queue,
            .features =
                {
                    features::Transform::create(),
                    features::BaseColor::create(),
                },
        }),
        EguiRenderPass::createPipelineStep(uiQueue),
    };

    initUI();
  }

  struct TestUIPanel : public VUIPanel {
    VUIArgs eguiArgs0;
    void (*rustFn)(VUIArgs &) = &vui_1;

    TestUIPanel() {
      eguiArgs0 = VUIArgs{
          .instance = vui_new_instance(),
      };
    }
    ~TestUIPanel() { vui_drop_instance(eguiArgs0.instance); }
    TestUIPanel(const TestUIPanel &) = delete;
    TestUIPanel &operator=(const TestUIPanel &) = delete;

    void render(void *renderContext, const egui::Input &inputs, shards::vui::PanelRenderCallback render) override {
      eguiArgs0.renderContext = renderContext;
      eguiArgs0.render = render;
      eguiArgs0.input = &inputs;
      rustFn(eguiArgs0);
    }
  };

  std::vector<std::shared_ptr<TestUIPanel>> uiInstances;
  const float createNewUIDelay = 0.3f;
  float counter = createNewUIDelay;

  VUIContext vuiCtx;

  void initUI() {
    auto panel = std::make_shared<TestUIPanel>();
    panel->rustFn = &vui_0;
    auto rot = linalg::rotation_quat(float3(0, 1, 0), gfx::degToRad(30.0f));
    panel->transform = linalg::mul(linalg::rotation_matrix(rot), panel->transform);
    panel->transform = linalg::mul(linalg::translation_matrix(float3(0, 0, 0)), panel->transform);
    panel->size = float2(1.0f, 1.4f);
    vuiCtx.panels.push_back(panel);

    panel = std::make_shared<TestUIPanel>();
    panel->rustFn = &vui_1;
    rot = linalg::rotation_quat(float3(0, 1, 0), gfx::degToRad(20.0f));
    panel->transform = linalg::mul(linalg::rotation_matrix(rot), panel->transform);
    panel->transform = linalg::mul(linalg::translation_matrix(float3(1, 0, 0)), panel->transform);
    panel->size = float2(1.6f, 1.4f);
    panel->alignment = float2(0.5f, 0.0f);
    vuiCtx.panels.push_back(panel);

    panel = std::make_shared<TestUIPanel>();
    panel->rustFn = &vui_2;
    rot = linalg::rotation_quat(float3(0, 1, 0), gfx::degToRad(0.0f));
    panel->transform = linalg::mul(linalg::rotation_matrix(rot), panel->transform);
    panel->transform = linalg::mul(linalg::translation_matrix(float3(0, 0, 0.5f)), panel->transform);
    panel->size = float2(0.5f, 0.5f);
    panel->alignment = float2(0.0f, 0.0f);
    vuiCtx.panels.push_back(panel);

    panel = std::make_shared<TestUIPanel>();
    panel->rustFn = &vui_0;
    rot = linalg::rotation_quat(float3(0, 1, 0), gfx::degToRad(180.0f));
    panel->transform = linalg::mul(linalg::rotation_matrix(rot), panel->transform);
    panel->transform = linalg::mul(linalg::translation_matrix(float3(0, 0, 1.0f)), panel->transform);
    panel->size = float2(1.5f, 0.5f);
    panel->alignment = float2(1.0f, 0.0f);
    vuiCtx.panels.push_back(panel);
  }

  float camRotation = 0.0f;
  bool kLeft{}, kRight{};
  void updateView(double time) {
    const float camSpeed = pi * 0.5f;
    if (kLeft)
      camRotation -= camSpeed * deltaTime;
    if (kRight)
      camRotation += camSpeed * deltaTime;

    const float3 p = float3(0, 3, 3);
    float4 q = linalg::rotation_quat(float3(0, 1, 0), camRotation);
    float3 p1 = linalg::qrot(q, p);
    view->view = linalg::lookat_matrix(p1, float3(0, 0, 0), float3(0, 1, 0));
  }

  void renderFrame(double time, float deltaTime) {
    updateView(time);

    renderer->beginFrame();

    queue->clear();
    uiQueue->clear();

    float2 viewSize = float2(window.getDrawableSize());
    float2 inputToViewScale = viewSize / float2(window.getSize());
    vuiCtx.prepareInputs(inputBuffer, inputToViewScale, gfx::SizedView(view, viewSize));
    vuiCtx.evaluate(uiQueue, time, deltaTime);

    gr.begin(view, viewSize);
    auto &sr = gr.getShapeRenderer();
    sr.addLine(float3(0, 0, 0), float3(1, 0, 0) * 2.0f, float4(1, 0, 0, 0.7f), 1);
    sr.addLine(float3(0, 0, 0), float3(0, 1, 0) * 2.0f, float4(0, 1, 0, 0.7f), 1);
    sr.addLine(float3(0, 0, 0), float3(0, 0, 1) * 2.0f, float4(0, 0, 1, 0.7f), 1);
    vuiCtx.renderDebug(sr);
    gr.end(queue);

    // counter += deltaTime;
    // if (counter >= createNewUIDelay) {
    //   counter -= createNewUIDelay;
    //   if (uiInstances.size() < 8) {
    //     float2 offset = float2(2.0f, 1.0f) * uiInstances.size();
    //     uiInstances.emplace_back(std::make_shared<TestUIPanel>(float4(offset.x, offset.y, 200.0f + offset.x, 200.0f +
    //     offset.y))); SPDLOG_INFO("Now at {} egui instances", uiInstances.size());
    //   }
    // }

    // for (auto &inst : uiInstances) {
    //   inst->eval(queue);
    // }

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
          else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            if (event.key.keysym.sym == SDLK_LEFT) {
              kLeft = event.key.type == SDL_KEYDOWN;
            } else if (event.key.keysym.sym == SDLK_RIGHT) {
              kRight = event.key.type == SDL_KEYDOWN;
            }
          }
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

extern "C" int main(int, char **) {
  App instance;
  instance.init(nullptr);
  instance.runMainLoop();
  return 0;
}
