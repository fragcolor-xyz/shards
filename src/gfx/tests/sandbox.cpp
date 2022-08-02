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
#include <gfx/helpers/shapes.hpp>
#include <gfx/helpers/translation_gizmo.hpp>
#include <gfx/imgui/imgui.hpp>
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

struct App {
  Window window;
  Context context;

  Loop loop;
  float deltaTime;

  ViewPtr view;
  std::shared_ptr<Renderer> renderer;
  std::shared_ptr<ImGuiRenderer> imgui;

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();
  PipelineSteps pipelineSteps;

  gizmos::InputState gizmoInputState;
  gizmos::Context gizmoContext;
  gizmos::TranslationGizmo translationGizmo;
  float4x4 gizmoTransform = linalg::identity;

  DrawableHierarchyPtr duck;
  InputBuffer inputBuffer;

  App() {}

  void init(const char *hostElement) {
    window.init(WindowCreationOptions{.width = 1280, .height = 720});

    gfx::ContextCreationOptions contextOptions = {};
    contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
    context.init(window, contextOptions);

    renderer = std::make_shared<Renderer>(context);
    imgui = std::make_shared<ImGuiRenderer>(context);
    {
      auto &io = ImGui::GetIO();
      io.Fonts->Clear();
      ImFontConfig config{};
      config.SizePixels = std::floor(13.0f * 1.5f);
      io.Fonts->AddFontDefault(&config);
    }

    view = std::make_shared<View>();
    view->view = linalg::lookat_matrix(float3(3.0f, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
    view->proj = ViewPerspectiveProjection{
        degToRad(45.0f),
        FovDirection::Horizontal,
    };

    auto glbPath = gfx::resolveDataPath(fmt::format("external/glTF-Sample-Models/2.0/{0}/glTF-Binary/{0}.glb", "Duck"));
    duck = loadGltfFromFile(glbPath.string().c_str());

    pipelineSteps = {
        makeDrawablePipelineStep(RenderDrawablesStep{
            .drawQueue = queue,
            .features =
                {
                    features::Transform::create(),
                    features::BaseColor::create(),
                },
        }),
        makeDrawablePipelineStep(RenderDrawablesStep{
            .drawQueue = editorQueue,
            .features =
                {
                    features::Transform::create(),
                    features::BaseColor::create(),
                },
            .forceDepthClear = false,
        }),
    };
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
    gizmoInputState.viewSize = window.getSize();
  }

  void renderFrame(float time, float deltaTime) {
    renderer->beginFrame();

    const float3 p = float3(0, 3, 3);

    float4 q = linalg::rotation_quat(float3(0, 1, 0), time * 0.1f);
    float3 p1 = linalg::qrot(q, p);

    view->view = linalg::lookat_matrix(p1, float3(0, 0, 0), float3(0, 1, 0));

    updateGizmoInput();

    gizmoContext.begin(gizmoInputState, view);

    translationGizmo.transform = gizmoTransform;
    gizmoContext.updateGizmo(translationGizmo);
    gizmoTransform = translationGizmo.transform;

    // Axis lines
    auto &sr = gizmoContext.renderer.getShapeRenderer();
    sr.addLine(float3(0, 0, 0), float3(1, 0, 0) * 2.0f, float4(1, 0, 0, 1), 1);
    sr.addLine(float3(0, 0, 0), float3(0, 1, 0) * 2.0f, float4(0, 1, 0, 1), 1);
    sr.addLine(float3(0, 0, 0), float3(0, 0, 1) * 2.0f, float4(0, 0, 1, 1), 1);

    gizmoContext.end(editorQueue);

    duck->transform = linalg::mul(gizmoTransform, linalg::scaling_matrix(float3(1.0f / 500.0f)));
    queue->add(duck);

    renderer->render(view, pipelineSteps);
    renderer->endFrame();
  }

  void renderDebugInfoWindow() {
    if (ImGui::Begin("Debug Info")) {
      static MovingAverage ma(16);
      ma.add(deltaTime);
      ImGui::LabelText("FPS", "%.2f", 1.0f / ma.getAverage());

      int2 outputSize = context.getMainOutputSize();
      ImGui::LabelText("Main Output Size", "%d x %d", outputSize.x, outputSize.y);
      ImGui::LabelText("Main Output Format", "%s", magic_enum::enum_name(context.getMainOutputFormat()).data());

      auto &io = ImGui::GetIO();
      ImGui::LabelText("io.MouseDelta", "(%.2f, %.2f)", io.MouseDelta.x, io.MouseDelta.x);
      ImGui::LabelText("io.MousePos", "(%.2f, %.2f)", io.MousePos.x, io.MousePos.x);
      ImGui::LabelText("io.MouseDown", "%d %d %d %d %d", io.MouseDown[0], io.MouseDown[1], io.MouseDown[2], io.MouseDown[3],
                       io.MouseDown[4]);
    }
    ImGui::End();
  }

  void layoutUI() {
    renderDebugInfoWindow();

    ImGuiIO &io = ImGui::GetIO();
    ConsumeEventFilter consumeEvents = ConsumeEventFilter::None;
    if (io.WantCaptureMouse)
      consumeEvents = consumeEvents | ConsumeEventFilter::PointerDown;
    if (io.WantCaptureKeyboard)
      consumeEvents = consumeEvents | ConsumeEventFilter::Keyboard | ConsumeEventFilter::Controller;
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
        }
      }
      osYield();
    }
  }
};

int main() {
  App instance;
  instance.init(nullptr);
  instance.runMainLoop();
  return 0;
}
