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
#include <magic_enum.hpp>
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
        }),
    };
  }

  // std::vector<gizmos::HandlePtr> handles = {
  //     std::make_shared<gizmos::Handle>(),
  //     std::make_shared<gizmos::Handle>(),
  //     std::make_shared<gizmos::Handle>(),
  // };
  // float4x4 gizmoTransform = linalg::identity;
  // void setupGizmos() {
  //   struct Callbacks : public gizmos::IGizmoCallbacks {
  //     std::vector<gizmos::Handle *> handles;
  //     App *app;
  //     float4x4 dragStartTransform;
  //     float3 dragStartPoint;

  //     size_t getHandleIndex(gizmos::Handle &inHandle) {
  //       for (size_t i = 0; i < 3; i++) {
  //         gizmos::Handle &handle = *handles[i];
  //         if (&handle == &inHandle) {
  //           return i;
  //         }
  //       }
  //       throw std::logic_error("Invalid handle");
  //     }

  //     float3 getAxis(size_t index) {
  //       float3 base{};
  //       base[index] = 1.0f;

  //       return linalg::mul(dragStartTransform, float4(base, 0)).xyz();
  //     }

  //     virtual void grabbed(gizmos::Handle &handle, gizmos::Context &context) {
  //       dragStartTransform = app->gizmoTransform;

  //       size_t index = getHandleIndex(handle);
  //       SPDLOG_INFO("Handle {} ({}) grabbed", index, getAxis(index));

  //       dragStartPoint =
  //           hitOnPlane(context.eyeLocation, context.rayDirection, extractTranslation(app->gizmoTransform), getAxis(index));
  //     }

  //     virtual void released(gizmos::Handle &handle, gizmos::Context &context) {
  //       size_t index = getHandleIndex(handle);
  //       SPDLOG_INFO("Handle {} ({}) released", index, getAxis(index));
  //     }

  //     virtual void move(gizmos::Handle &inHandle, gizmos::Context &context) {
  //       float3 fwd = getAxis(getHandleIndex(inHandle));

  //       // float3 planeT0 = fwd;
  //       // float3 planeNormal = dragStartPoint - context.eyeLocation;
  //       // planeNormal -= linalg::dot(planeNormal, planeT0) * planeT0;
  //       // planeNormal = linalg::normalize(planeNormal);

  //       // float3 planePoint = extractTranslation(app->gizmoTransform);

  //       auto &sr = app->gizmoRenderer.getShapeRenderer();

  //       float3 hitPoint = hitOnPlane(context.eyeLocation, context.rayDirection, dragStartPoint, fwd);

  //       // Intersect view ray with movement plane and project onto axis
  //       // float d;
  //       // if (intersectPlane(context.eyeLocation, context.rayDirection, planePoint, planeNormal, d)) {
  //       //   float3 hitPoint = context.eyeLocation + d * context.rayDirection;
  //       //   hitPoint = projectOntoAxis(hitPoint, extractTranslation(app->gizmoTransform), fwd);

  //       float3 delta = hitPoint - dragStartPoint;
  //       app->gizmoTransform = linalg::mul(linalg::translation_matrix(delta), dragStartTransform);

  //       sr.addLine(dragStartPoint, hitPoint, float4(1, 1, 1, 1), 1);

  //       sr.addPoint(hitPoint, float4(0, 1, 0, 1), 6);
  //       // }

  //       sr.addPoint(dragStartPoint, float4(0, 1, 0, 1), 6);
  //     }
  //   };
  //   auto callbacks = std::make_shared<Callbacks>();
  //   callbacks->app = this;
  //   for (auto &handle : handles) {
  //     gizmoContext.handles.push_back(handle);
  //     callbacks->handles.push_back(handle.get());
  //     handle->callbacks = callbacks;
  //   }
  // }

  // void updateGizmos(GizmoRenderer &gr) {
  //   auto &sr = gr.getShapeRenderer();

  //   for (size_t i = 0; i < 3; i++) {
  //     auto &handle = *handles[i].get();
  //     size_t axisIndex = i;

  //     float3 fwd{};
  //     fwd[axisIndex] = 1.0f;
  //     float3 t1 = float3(-fwd.z, -fwd.x, fwd.y);
  //     float3 t2 = linalg::cross(fwd, t1);
  //     float width = 0.1f;
  //     float length = 1.0f;

  //     auto &min = handle.selectionBox.min;
  //     auto &max = handle.selectionBox.max;
  //     min = -t1 * width - t2 * width;
  //     max = t1 * width + t2 * width + fwd * length;

  //     float3 center = (max + min) / 2;
  //     float3 size = max - min;
  //     handle.selectionBoxTransform = gizmoTransform;

  //     // Debug draw
  //     float4 color = float4(.7, .7, .7, 1.);
  //     uint32_t thickness = 1;
  //     if (gizmoContext.hovered && gizmoContext.hovered.get() == &handle) {
  //       color = float4(.5, 1., .5, 1.);
  //       thickness = 2;
  //     }
  //     sr.addBox(handle.selectionBoxTransform, center, size, color, thickness);
  //   }
  // }

  void renderFrame(float time, float deltaTime) {
    renderer->beginFrame();

    // queue->add(duck);
    const float3 p = float3(0, 3, 3);

    float4 q = linalg::rotation_quat(float3(0, 1, 0), time * 0.1f);
    float3 p1 = linalg::qrot(q, p);

    view->view = linalg::lookat_matrix(p1, float3(0, 0, 0), float3(0, 1, 0));

    gizmoContext.begin(gizmoInputState, view);

    translationGizmo.transform = gizmoTransform;
    gizmoContext.updateGizmo(translationGizmo);
    gizmoTransform = translationGizmo.transform;

    gizmoContext.end(editorQueue);

    // Axis lines
    // sr.addLine(float3(0, 0, 0), float3(1, 0, 0) * 2.0f, float4(1, 0, 0, 1), 1);
    // sr.addLine(float3(0, 0, 0), float3(0, 1, 0) * 2.0f, float4(0, 1, 0, 1), 1);
    // sr.addLine(float3(0, 0, 0), float3(0, 0, 1) * 2.0f, float4(0, 0, 1, 1), 1);

    // updateGizmos(gr);
    // gizmoContext.update(gizmoInputState, view);

    // float3 mousePoint = gizmoContext.eyeLocation + gizmoContext.rayDirection * 0.5f;
    // sr.addPoint(mousePoint, float4(1, 0, 1, 1), 6);

    // gr.end(editorQueue);

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

      ImGui::End();
    }
  }

  void renderUI(const std::vector<SDL_Event> &events) {
    imgui->beginFrame(events);
    renderDebugInfoWindow();

    imgui->endFrame();
  }

  void runMainLoop() {
    bool quit = false;
    int2 mousePos{};
    uint32_t mouseButtonState{};

    while (!quit) {
      if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {
        std::vector<SDL_Event> events;
        window.pollEvents(events);
        for (auto &event : events) {
          if (event.type == SDL_WINDOWEVENT) {
            if (event.window.type == SDL_WINDOWEVENT_SIZE_CHANGED) {
            }
          }
          if (event.type == SDL_QUIT)
            quit = true;

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

        context.resizeMainOutputConditional(window.getDrawableSize());

        gizmoInputState.pressed = (mouseButtonState & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        gizmoInputState.cursorPosition = float2(mousePos);
        gizmoInputState.viewSize = window.getSize();

        if (context.beginFrame()) {
          renderer->beginFrame();

          queue->clear();
          editorQueue->clear();

          renderFrame(loop.getAbsoluteTime(), deltaTime);

          renderer->endFrame();

          renderUI(events);

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
