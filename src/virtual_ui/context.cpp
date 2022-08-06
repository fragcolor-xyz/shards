#include "context.hpp"
#include <spdlog/spdlog.h>
#include <gfx/linalg.hpp>
#include <gfx/view_raycast.hpp>
#include <gfx/helpers/shapes.hpp>
#include <input/input.hpp>
#include <float.h>

using namespace gfx;
using shards::input::ConsumeEventFilter;
namespace shards::vui {

void Context::prepareInputs(input::InputBuffer &input, gfx::float2 inputToViewScale, const gfx::SizedView &sizedView) {
  pointerInputs.clear();
  otherEvents.clear();
  for (auto it = input.begin(); it; ++it) {
    bool isPointerEvent = false;
    float2 pointerCoords;
    switch (it->type) {
    case SDL_MOUSEMOTION:
      pointerCoords = float2(it->motion.x, it->motion.y);
      isPointerEvent = true;
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      pointerCoords = float2(it->button.x, it->button.y);
      isPointerEvent = true;
      break;
    }

    if (isPointerEvent) {
      PointerInput outEvt;
      outEvt.iterator = it;
      outEvt.ray = sizedView.getRay(pointerCoords * inputToViewScale);
      pointerInputs.push_back(outEvt);
    } else {
      otherEvents.push_back(it);
    }
  }

  struct PanelInput {
    std::vector<egui::InputEvent> inputEvents;
  };

  std::vector<egui::InputEvent> panelEvents;

  for (auto &evt : pointerInputs) {
    for (size_t panelIndex = 0; panelIndex < panels.size(); panelIndex++) {
      auto &panel = panels[panelIndex];

      float3 planePoint = extractTranslation(panel->transform);
      float3 planeNormal = panel->transform.z.xyz();

      float dist{};
      if (gfx::intersectPlane(evt.ray.origin, evt.ray.direction, planePoint, planeNormal, dist) && dist < evt.hitDistance) {

        float3 hitPoint = evt.ray.origin + evt.ray.direction * dist;

        auto geom = panel->getGeometry();
        float3 topLeft = geom.getTopLeft();
        float fX = linalg::dot(hitPoint, geom.right) - linalg::dot(topLeft, geom.right);
        float fY = -(linalg::dot(hitPoint, geom.up) - linalg::dot(topLeft, geom.up));
        if (fX >= 0.0f && fX <= panel->size.x) {
          if (fY >= 0.0f && fY <= panel->size.y) {
            evt.hitDistance = dist;
            evt.panelCoord = float2(fX, fY);
            focusedPanel = evt.hitPanel = panel;
          }
        }
      }
    }
  }
}

void Context::renderDebug(gfx::ShapeRenderer &sr) {
  for (const auto &panel : panels) {
    auto geom = panel->getGeometry();
    float4 c = panel == focusedPanel ? float4(0, 1, 0, 1) : float4(0.8, 0.8, 0.8, 1.0);
    sr.addRect(geom.center, geom.right, geom.up, geom.size, c, 2);
    // panel->transform
  }

  for (const auto &pointerInput : pointerInputs) {
    if (pointerInput.hitPanel) {
      auto geom = pointerInput.hitPanel->getGeometry();
      float3 hitCoord = geom.getTopLeft() + pointerInput.panelCoord.x * geom.right - pointerInput.panelCoord.y * geom.up;
      sr.addPoint(hitCoord, float4(1, 1, 1, 1), 5);
    }
  }
}

struct ContextCachedPanel {
  gfx::EguiRenderer renderer;
};

struct RenderContext {
  PanelPtr panel;
  gfx::DrawQueuePtr queue;
  std::shared_ptr<ContextCachedPanel> cached;
  float scaling;
};

static void renderPanel(const RenderContext &ctx, const egui::FullOutput &output) {
  EguiRenderer &renderer = ctx.cached->renderer;
  PanelPtr panel = ctx.panel;

  float4x4 rootTransform = linalg::identity;
  PanelGeometry geom = panel->getGeometry();

  rootTransform = linalg::mul(linalg::scaling_matrix(float3(1.0f / ctx.scaling)), rootTransform);

  // Derive rotation matrix from panel right/up
  float4x4 rotationMat;
  rotationMat.x = float4(geom.right, 0);
  rotationMat.y = float4(-geom.up, 0);
  rotationMat.z = float4(linalg::cross(geom.right, geom.up), 0);
  rotationMat.w = float4(0, 0, 0, 1);
  rootTransform = linalg::mul(rotationMat, rootTransform);

  // Place drawable at top-left corner of panel UI
  rootTransform = linalg::mul(linalg::translation_matrix(geom.getTopLeft()), rootTransform);

  renderer.render(output, rootTransform, ctx.queue);
}

void Context::evaluate(gfx::DrawQueuePtr queue, double time, float deltaTime) {
  for (auto &panel : panels) {
    // Points per world space unit
    float virtualPointScale = 200.0f;
    float pixelsPerPoint = 4.0f;

    eguiInputTranslator.begin(time, deltaTime);
    for (auto &pointerInput : pointerInputs) {
      if (pointerInput.hitPanel == panel) {
        const SDL_Event &sdlEvent = *pointerInput.iterator;
        switch (sdlEvent.type) {
        case SDL_MOUSEMOTION: {
          egui::InputEvent evt;
          auto &oevent = evt.pointerMoved;
          oevent.type = egui::InputEventType::PointerMoved;
          oevent.pos = egui::toPos2(pointerInput.panelCoord * virtualPointScale);
          eguiInputTranslator.pushEvent(evt);
          break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
          egui::InputEvent evt;
          auto &ievent = sdlEvent.button;
          auto &oevent = evt.pointerButton;
          oevent.type = egui::InputEventType::PointerButton;
          oevent.pos = egui::toPos2(pointerInput.panelCoord * virtualPointScale);
          oevent.pressed = sdlEvent.type == SDL_MOUSEBUTTONDOWN;
          bool unknownButton = false;
          switch (ievent.button) {
          case SDL_BUTTON_LEFT:
            oevent.button = egui::PointerButton::Primary;
            break;
          case SDL_BUTTON_MIDDLE:
            oevent.button = egui::PointerButton::Middle;
            break;
          case SDL_BUTTON_RIGHT:
            oevent.button = egui::PointerButton::Secondary;
            break;
          default:
            unknownButton = true;
            break;
          }
          if (unknownButton)
            break;
          eguiInputTranslator.pushEvent(evt);
          break;
        }
        }
      }
    }

    if (panel == focusedPanel) {
      for (auto &otherEvent : otherEvents) {
        eguiInputTranslator.translateEvent(*otherEvent);
      }
    }
    eguiInputTranslator.end();

    egui::Input input = *eguiInputTranslator.getOutput();
    input.screenRect.min = egui::Pos2{};
    input.screenRect.max = egui::toPos2(panel->size * virtualPointScale);
    input.pixelsPerPoint = pixelsPerPoint;

    RenderContext ctx{
        .panel = panel,
        .queue = queue,
        .cached = getCachedPanel(panel),
        .scaling = virtualPointScale,
    };
    panel->render(&ctx, input, (PanelRenderCallback)&renderPanel);
  }
}

std::shared_ptr<ContextCachedPanel> Context::getCachedPanel(PanelPtr panel) {
  auto it = cachedPanels.find(panel.get());
  if (it == cachedPanels.end()) {
    it = cachedPanels.insert(std::make_pair(panel.get(), std::make_shared<ContextCachedPanel>())).first;
  }
  return it->second;
}

} // namespace shards::vui
