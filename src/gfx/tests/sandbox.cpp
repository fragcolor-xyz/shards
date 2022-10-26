#include "linalg/linalg.h"
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
#include <gfx/gltf/gltf.hpp>
#include <gfx/gizmos/shapes.hpp>
#include <gfx/gizmos/translation_gizmo.hpp>
#include <gfx/linalg.hpp>
#include <gfx/loop.hpp>
#include <gfx/mesh.hpp>
#include <gfx/moving_average.hpp>
#include <gfx/paths.hpp>
#include <gfx/renderer.hpp>
#include <gfx/types.hpp>
#include <gfx/utils.hpp>
#include <gfx/view.hpp>
#include <gfx/window.hpp>
#include <gfx/moving_average.hpp>
#include <input/input.hpp>
#include <magic_enum.hpp>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <log/log.hpp>

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

struct App {
  Window window;
  Context context;

  Loop loop;
  float deltaTime;

  ViewPtr view;
  std::shared_ptr<Renderer> renderer;

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();
  PipelineSteps pipelineSteps;

  gizmos::InputState gizmoInputState;
  gizmos::Context gizmoContext;

  gizmos::TranslationGizmo translationGizmo0;
  gizmos::TranslationGizmo translationGizmo1;

  DrawableHierarchyPtr duck;
  InputBuffer inputBuffer;

  App() {}

  void init(const char *hostElement) {
    window.init(WindowCreationOptions{.width = 1280, .height = 720});

    gfx::ContextCreationOptions contextOptions = {};
    contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
    context.init(window, contextOptions);

    renderer = std::make_shared<Renderer>(context);

    view = std::make_shared<View>();
    view->view = linalg::lookat_matrix(float3(3.0f, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
    view->proj = ViewPerspectiveProjection{
        degToRad(45.0f),
        FovDirection::Horizontal,
    };

    auto glbPath = gfx::resolveDataPath(fmt::format("external/glTF-Sample-Models/2.0/{0}/glTF-Binary/{0}.glb", "Duck"));
    duck = loadGltfFromFile(glbPath.string().c_str());

    pipelineSteps = {
        makePipelineStep(RenderDrawablesStep{
            .drawQueue = queue,
            .features =
                {
                    features::Transform::create(),
                    features::BaseColor::create(),
                },
        }),
        makePipelineStep(RenderDrawablesStep{
            .drawQueue = editorQueue,
            .features =
                {
                    features::Transform::create(),
                    features::BaseColor::create(),
                },
            .forceDepthClear = false,
        }),
    };

    translationGizmo0.scale = 0.75f;
    translationGizmo1.scale = 0.5f;

    translationGizmo0.transform = linalg::mul(linalg::translation_matrix(float3(-1, 0, 0)), translationGizmo0.transform);

    float4 rotY = linalg::rotation_quat(float3(0, 1, 0), degToRad(45.0f));
    translationGizmo1.transform = linalg::rotation_matrix(rotY);
    translationGizmo1.transform = linalg::mul(linalg::translation_matrix(float3(1, 0, 0)), translationGizmo1.transform);
  }

  int2 mousePos{};
  uint32_t mouseButtonState{};
  void updateGizmoInput() {
    for (auto &event : inputBuffer) {
      if (event.type == SDL_MOUSEMOTION) {
        mousePos.x = event.motion.x;
        mousePos.y = event.motion.y;
      } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        mousePos.x = event.button.x;
        mousePos.y = event.button.y;
        if (event.button.state == SDL_PRESSED)
          mouseButtonState |= SDL_BUTTON(event.button.button);
        else
          mouseButtonState &= ~SDL_BUTTON(event.button.button);
      }
    }

    gizmoInputState.pressed = (mouseButtonState & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    gizmoInputState.cursorPosition = float2(mousePos);
    gizmoInputState.viewSize = float2(window.getSize());
  }

  void renderFrame(float time, float deltaTime) {
    const float3 p = float3(0, 3, 3);

    float4 q = linalg::rotation_quat(float3(0, 1, 0), time * 0.1f);
    float3 p1 = linalg::qrot(q, p);

    view->view = linalg::lookat_matrix(p1, float3(0, 0, 0), float3(0, 1, 0));

    updateGizmoInput();

    gizmoContext.begin(gizmoInputState, view);

    gizmoContext.updateGizmo(translationGizmo0);
    gizmoContext.updateGizmo(translationGizmo1);

    // Axis lines
    auto &sr = gizmoContext.renderer.getShapeRenderer();
    sr.addLine(float3(0, 0, 0), float3(1, 0, 0) * 2.0f, float4(1, 0, 0, 1), 1);
    sr.addLine(float3(0, 0, 0), float3(0, 1, 0) * 2.0f, float4(0, 1, 0, 1), 1);
    sr.addLine(float3(0, 0, 0), float3(0, 0, 1) * 2.0f, float4(0, 0, 1, 1), 1);

    gizmoContext.end(editorQueue);

    renderer->render(view, pipelineSteps);
  }

  void layoutUI() {
    ConsumeEventFilter consumeEvents = ConsumeEventFilter::None;
    inputBuffer.consumeEvents(consumeEvents);
  }

  void runMainLoop() {
    bool quit = false;

    while (!quit) {
      if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {
        inputBuffer.clear();
        window.pollEventsForEach([&](auto evt) { inputBuffer.push_back(evt); });
        for (auto &event : inputBuffer) {
          if (event.type == SDL_WINDOWEVENT) {
            if (event.window.type == SDL_WINDOWEVENT_SIZE_CHANGED) {
            }
          }
          if (event.type == SDL_QUIT)
            quit = true;
        }

        context.resizeMainOutputConditional(window.getDrawableSize());

        if (context.beginFrame()) {
          renderer->beginFrame();
          imgui->beginFrame(inputBuffer);

          queue->clear();
          editorQueue->clear();

          // Process UI input before scene
          layoutUI();

          renderFrame(loop.getAbsoluteTime(), deltaTime);

          renderer->endFrame();

          // Render UI after main render
          imgui->endFrame();

          context.endFrame();

          static MovingAverage delaTimeMa(32);
          delaTimeMa.add(deltaTime);

          static float fpsCounter = 0.0f;
          fpsCounter += deltaTime;
          if (fpsCounter >= 1.0f) {
            SPDLOG_INFO("Average FPS: {:0.01f}", 1.0f / delaTimeMa.getAverage());
            fpsCounter = 0.0f;
          }
        }
      }
      osYield();
    }
  }
};

int main() {
  shards::logging::setupDefaultLoggerConditional();
  App instance;
  instance.init(nullptr);
  instance.runMainLoop();
  return 0;
}
