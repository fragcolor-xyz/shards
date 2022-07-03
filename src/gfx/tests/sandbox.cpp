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
                    ScreenSpaceSizeFeature::create(),
                    features::BaseColor::create(),
                },
        }),
    };
  }

  void renderFrame(float time, float deltaTime) {
    renderer->beginFrame();

    // queue->add(duck);

    const float3 p = float3(0, 3, 3);

    float4 q = linalg::rotation_quat(float3(0, 1, 0), time*0.1f);
    float3 p1 = linalg::qrot(q, p);
    view->view = linalg::lookat_matrix(p1, float3(0, 0, 0), float3(0, 1, 0));

    sr.begin();

    // size_t numSteps = 16;
    // float spacing = 1.0f / 8.0f;
    // for (size_t i = 0; i < numSteps; i++) {
    //   float sz = (float(i)) * spacing;
    //   sr.addLine(float3(0, 0, sz), float3(1, 0, sz), float4(1, 0, 0, 1), 1 + i);
    //   sr.addLine(float3(0, 0, sz), float3(1, 0, sz), float4(1, 0, 0, 1), 1 + i);

    //   sr.addLine(float3(sz, 0, 0), float3(sz, 1, 0), float4(0, 1, 0, 1), 1 + i);
    //   sr.addLine(float3(sz, 0, 0), float3(sz, 1, 0), float4(0, 1, 0, 1), 1 + i);

    //   sr.addLine(float3(0, sz, 0), float3(0, sz, 1), float4(0, 0, 1, 1), 1 + i);
    //   sr.addLine(float3(0, sz, 0), float3(0, sz, 1), float4(0, 0, 1, 1), 1 + i);
    // }

    // auto doTriangle = [&](float3 center, float3 x, float3 y, float4 color, uint32_t thickness) {
    //   float3 triangleVerts[3] = {};
    //   triangleVerts[0] = center - 0.25f * x;
    //   triangleVerts[1] = center + 0.25f * x;
    //   triangleVerts[2] = center + 0.25f * y;
    //   sr.addLine(triangleVerts[0], triangleVerts[1], color, thickness);
    //   sr.addLine(triangleVerts[1], triangleVerts[2], color, thickness);
    //   sr.addLine(triangleVerts[2], triangleVerts[0], color, thickness);
    // };

    // for (size_t i = 0; i < 9; i++) {
    //   float3 offset = float3(0, 0, 0.2f) * (1 + i);
    //   doTriangle(float3(0.5, 0.5, 0.0) + offset, float3(1, 0, 0), float3(0, 1, 0), float4(1, 1, 1, 1), 1 + i);
    // }

    for (size_t i = 1; i < 5; i++) {
      float r = 0.2f + (0.2f * i);
      float2 size = float2(r * 0.9f, r * 1.1f);
      uint32_t thickness = (1 + i);
      sr.addRect(float3(0, 0, 0), float3(1, 0, 0), float3(0, 0, 1), size, float4(0, 1, 0, 1), thickness);
      sr.addRect(float3(0, 0, 0), float3(0, 0, 1), float3(0, 1, 0), size, float4(1, 0, 0, 1), thickness);
      sr.addRect(float3(0, 0, 0), float3(1, 0, 0), float3(0, 1, 0), size, float4(0, 0, 1, 1), thickness);
    }

    sr.finish(editorQueue);

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
