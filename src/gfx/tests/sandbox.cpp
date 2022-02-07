#include "gfx/context.hpp"
#include "gfx/drawable.hpp"
#include "gfx/enums.hpp"
#include "gfx/features/base_color.hpp"
#include "gfx/features/debug_color.hpp"
#include "gfx/features/transform.hpp"
#include "gfx/geom.hpp"
#include "gfx/linalg.hpp"
#include "gfx/loop.hpp"
#include "gfx/mesh.hpp"
#include "gfx/moving_average.hpp"
#include "gfx/renderer.hpp"
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
void osYield() { emscripten_sleep(0); }
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

struct App {
  Window window;
  Loop loop;
  Context context;

  MeshPtr sphereMesh;
  MeshPtr cubeMesh;
  ViewPtr view;
  std::shared_ptr<Renderer> renderer;
  DrawQueue drawQueue;

  PipelineSteps pipelineSteps;

  App() {}

  void init(const char *hostElement) {
    spdlog::debug("sandbox Init");
    window.init(WindowCreationOptions{.width = 512, .height = 512});

    gfx::ContextCreationOptions contextOptions = {};
    contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
    contextOptions.debug = true;
    context.init(window, contextOptions);

    renderer = std::make_shared<Renderer>(context);

    FeaturePtr blendFeature = std::make_shared<Feature>();
    blendFeature->state.set_blend(BlendState{
        .color = BlendComponent::AlphaPremultiplied,
        .alpha = BlendComponent::Opaque,
    });

    pipelineSteps.emplace_back(makeDrawablePipelineStep(RenderDrawablesStep{
        .features =
            {
                features::Transform::create(),
                features::BaseColor::create(),
            },
        // .sortMode = SortMode::BackToFront,
    }));

    view = std::make_shared<View>();
    view->proj = ViewPerspectiveProjection{};
    view->view = linalg::lookat_matrix(float3(0, 30, 100), float3(0, 0, 0.0f), float3(0, 1, 0));

    geom::SphereGenerator sphere;
    sphere.generate();

    sphereMesh = createMesh(sphere.vertices, sphere.indices);
    buildDrawables();

    rnd.seed(time(nullptr));
  }

  std::default_random_engine rnd;
  std::vector<DrawablePtr> testDrawables;
  void buildDrawables() {
    testDrawables.clear();

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    int2 testGridDim = {64, 64};
    for (size_t y = 0; y < testGridDim.y; y++) {
      float fy = (y - float(testGridDim.y) / 2.0f) * 2.0f;
      for (size_t x = 0; x < testGridDim.x; x++) {
        float fx = (x - float(testGridDim.x) / 2.0f) * 2.0f;
        auto drawable = std::make_shared<Drawable>(sphereMesh);
        drawable->transform = linalg::translation_matrix(float3(fx, 0.0f, fy));

        float4 color(dist(rnd), dist(rnd), dist(rnd), 1.0f);
        drawable->parameters.basic.insert_or_assign("baseColor", color);
        testDrawables.push_back(drawable);
      }
    }
  }

  void renderFrame() {
    buildDrawables();
    for (auto &drawable : testDrawables) {
      drawQueue.add(drawable);
    }

    renderer->render(drawQueue, view, pipelineSteps);
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
        context.beginFrame();
        renderer->swapBuffers();
        drawQueue.clear();

        renderFrame();

        context.endFrame();

        static MovingAverage delaTimeMa(32);
        delaTimeMa.add(deltaTime);

        static float fpsCounter = 0.0f;
        fpsCounter += deltaTime;
        if (fpsCounter >= 1.0f) {
          spdlog::info("Average FPS: {:0.01f}", 1.0f / delaTimeMa.getAverage());
          fpsCounter = 0.0f;
        }
      } else {
        osYield();
      }
    }
  }
};

int main() {
  App instance;
  instance.init(nullptr);
  instance.runMainLoop();
  return 0;
}
