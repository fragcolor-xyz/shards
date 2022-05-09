#include "gfx/context.hpp"
#include "gfx/drawable.hpp"
#include "gfx/enums.hpp"
#include "gfx/features/base_color.hpp"
#include "gfx/features/debug_color.hpp"
#include "gfx/features/transform.hpp"
#include "gfx/geom.hpp"
#include "gfx/imgui/imgui.hpp"
#include "gfx/linalg.hpp"
#include "gfx/loop.hpp"
#include "gfx/mesh.hpp"
#include "gfx/moving_average.hpp"
#include "gfx/paths.hpp"
#include "gfx/renderer.hpp"
#include "gfx/texture.hpp"
#include "gfx/texture_file/texture_file.hpp"
#include "gfx/types.hpp"
#include "gfx/utils.hpp"
#include "gfx/view.hpp"
#include "gfx/window.hpp"
#include "linalg/linalg.h"
#include "spdlog/spdlog.h"
#include <SDL_events.h>
#include <SDL_video.h>
#include <cassert>
#include <cstring>
#include <exception>
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

struct VertexPC {
  float position[3];
  float color[4];

  VertexPC() = default;
  VertexPC(const geom::VertexPNT &other) {
    setPosition(*(float3 *)other.position);
    setColor(float4(1, 1, 1, 1));
  }

  void setPosition(const float3 &v) { memcpy(position, &v.x, sizeof(float) * 3); }
  void setColor(const float4 &v) { memcpy(color, &v.x, sizeof(float) * 4); }

  static std::vector<MeshVertexAttribute> getAttributes() {
    std::vector<MeshVertexAttribute> attribs;
    attribs.emplace_back("position", 3, VertexAttributeType::Float32);
    attribs.emplace_back("color", 4, VertexAttributeType::Float32);
    return attribs;
  }
};

template <typename T> std::vector<T> convertVertices(const std::vector<geom::VertexPNT> &input) {
  std::vector<T> result;
  for (auto &vert : input)
    result.emplace_back(vert);
  return result;
}

template <typename T> MeshPtr createMesh(const std::vector<T> &verts, const std::vector<geom::GeneratorBase::index_t> &indices) {
  MeshPtr mesh = std::make_shared<Mesh>();
  MeshFormat format{
      .vertexAttributes = T::getAttributes(),
  };
  mesh->update(format, verts.data(), verts.size() * sizeof(T), indices.data(),
               indices.size() * sizeof(geom::GeneratorBase::index_t));
  return mesh;
}

MeshPtr createPlaneMesh() {
  geom::PlaneGenerator gen;
  gen.generate();
  return createMesh(gen.vertices, gen.indices);
}

struct App {
  Window window;
  Loop loop;
  Context context;

  ViewPtr view;
  std::shared_ptr<Renderer> renderer;
  std::shared_ptr<ImGuiRenderer> imgui;
  DrawQueue drawQueue;

  std::vector<TexturePtr> textures;

  PipelineSteps pipelineSteps;

  App() {}

  void init(const char *hostElement) {
    spdlog::debug("sandbox Init");
    window.init(WindowCreationOptions{.width = 1280, .height = 720});

    gfx::ContextCreationOptions contextOptions = {};
    contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
    contextOptions.debug = true;
    context.init(window, contextOptions);

    renderer = std::make_shared<Renderer>(context);
    imgui = std::make_shared<ImGuiRenderer>(context);

    pipelineSteps.emplace_back(makeDrawablePipelineStep(RenderDrawablesStep{
        .features =
            {
                features::Transform::create(),
                features::BaseColor::create(),
            },
        .sortMode = SortMode::BackToFront,
    }));

    view = std::make_shared<View>();
    view->proj = ViewOrthographicProjection{
        .size = 4.0f,
        .near = -10.0f,
        .far = 10.0f,
    };

    rnd.seed(time(nullptr));
    buildDrawables();
  }

  std::default_random_engine rnd;
  std::vector<DrawablePtr> testDrawables;

  void buildDrawables() {
    testDrawables.clear();

    MeshPtr planeMesh = createPlaneMesh();

    float4x4 transform;
    DrawablePtr drawable;
    transform = linalg::translation_matrix(float3(-.5f, 0.0f, 0.0f));
    drawable = std::make_shared<Drawable>(planeMesh, transform);
    drawable->parameters.set("baseColor", float4(1, 0, 0, 1));
    testDrawables.push_back(drawable);

    transform = linalg::translation_matrix(float3(.5f, 0.0f, 0.0f));
    drawable = std::make_shared<Drawable>(planeMesh, transform);
    drawable->parameters.set("baseColor", float4(0, 1, 0, 1));
    testDrawables.push_back(drawable);
  }

  void renderFrame(float time, float deltaTime) {
    for (auto &drawable : testDrawables) {
      drawQueue.add(drawable);
    }

    renderer->render(drawQueue, view, pipelineSteps);
  }

  void renderUI(const std::vector<SDL_Event> &events) {
    imgui->beginFrame(events);

    ImGui::ShowDemoWindow();

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

          drawQueue.clear();

          renderFrame(loop.getAbsoluteTime(), deltaTime);
          // renderUI(events);

          renderer->endFrame();
          context.endFrame();

          static MovingAverage delaTimeMa(32);
          delaTimeMa.add(deltaTime);

          static float fpsCounter = 0.0f;
          fpsCounter += deltaTime;
          if (fpsCounter >= 1.0f) {
            spdlog::info("Average FPS: {:0.01f}", 1.0f / delaTimeMa.getAverage());
            fpsCounter = 0.0f;
          }
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
