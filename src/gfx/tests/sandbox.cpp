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
#include <gfx/geom.hpp>
#include <gfx/gltf/gltf.hpp>
#include <gfx/helpers/shapes.hpp>
#include <gfx/imgui/imgui.hpp>
#include <gfx/linalg.hpp>
#include <gfx/loop.hpp>
#include <gfx/mesh.hpp>
#include <gfx/paths.hpp>
#include <gfx/renderer.hpp>
#include <gfx/types.hpp>
#include <gfx/utils.hpp>
#include <gfx/view.hpp>
#include <gfx/window.hpp>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace gfx;

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
  Loop loop;
  Context context;

  ViewPtr view;
  std::shared_ptr<Renderer> renderer;
  std::shared_ptr<ImGuiRenderer> imgui;

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();
  PipelineSteps pipelineSteps;
  ShapeRenderer sr;

  DrawableHierarchyPtr duck;

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
        }),
    };
  }

  GizmoRenderer gr;

  void renderFrame(float time, float deltaTime) {
    renderer->beginFrame();

    // queue->add(duck);
    const float3 p = float3(0, 3, 3);

    float4 q = linalg::rotation_quat(float3(0, 1, 0), time * 0.1f);
    float3 p1 = linalg::qrot(q, p);
    view->view = linalg::lookat_matrix(p1, float3(0, 0, 0), float3(0, 1, 0));

    gr.begin(view, context.getMainOutputSize());
    auto &sr = gr.getShapeRenderer();

    // Axis lines
    sr.addLine(float3(0, 0, 0), float3(1, 0, 0) * 2.0f, float4(1, 0, 0, 1), 1);
    sr.addLine(float3(0, 0, 0), float3(0, 1, 0) * 2.0f, float4(0, 1, 0, 1), 1);
    sr.addLine(float3(0, 0, 0), float3(0, 0, 1) * 2.0f, float4(0, 0, 1, 1), 1);

    // Gizmo handles
    float3 axisY = float3(0, 1, 0);
    float4 colors[3] = {
        float4(1, 0, 0, 1),
        float4(0, 1, 0, 1),
        float4(0, 0, 1, 1),
    };
    float4 bodyColor = float4(.5,.5,.5,1.);
    for (size_t i = 0; i < 5; i++) {
      float length = (0.2f + float(i) * 0.1f);
      float radius = 0.02f;
      float xOffset = 0.0f + float(i) * 0.1f;
      float3 pos = float3(xOffset, 0, 0);
      gr.addHandle(pos, axisY, radius, length, bodyColor, GizmoRenderer::CapType::Arrow, colors[i % 3]);
      gr.addHandle(pos + float3(0, 0, 0.5f), axisY, radius, length, bodyColor, GizmoRenderer::CapType::Cube, colors[i % 3]);
      gr.addHandle(pos + float3(0, 0, 1.0f), axisY, radius, length, bodyColor, GizmoRenderer::CapType::Sphere, colors[i % 3]);
    }

    gr.end(editorQueue);

    renderer->render(view, pipelineSteps);
    renderer->endFrame();
  }

  void renderUI(const std::vector<SDL_Event> &events) {
    imgui->beginFrame(events);
    imgui->endFrame();
  }

  void runMainLoop() {
    bool quit = false;
    while (!quit) {
      std::vector<SDL_Event> events;
      window.pollEvents(events);
      for (auto &event : events) {
        if (event.type == SDL_WINDOWEVENT) {
          if (event.window.type == SDL_WINDOWEVENT_SIZE_CHANGED) {
          }
        }
        if (event.type == SDL_QUIT)
          quit = true;
      }

      context.resizeMainOutputConditional(window.getDrawableSize());

      float deltaTime;
      if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {
        if (context.beginFrame()) {
          renderer->beginFrame();

          queue->clear();
          editorQueue->clear();

          renderFrame(loop.getAbsoluteTime(), deltaTime);

          renderer->endFrame();
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

#include "imgui/imgui_demo.cpp"
