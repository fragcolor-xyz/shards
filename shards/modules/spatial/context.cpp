#include "context.hpp"
#include <shards/modules/egui/egui_types.hpp>
#include <spdlog/spdlog.h>
#include <gfx/linalg.hpp>
#include <gfx/view_raycast.hpp>
#include <gfx/gizmos/shapes.hpp>
#include <input/input.hpp>
#include <float.h>
#include <magic_enum.hpp>

using namespace gfx;
namespace shards::spatial {

// Associated panel data
struct ContextCachedPanel {
  gfx::EguiRenderer renderer;
};

void Context::prepareInputs(input::InputBuffer &input, gfx::float2 inputToViewScale, const gfx::SizedView &sizedView) {
  this->sizedView = sizedView;

  pointerInputs.clear();
  otherEvents.clear();
  for (auto it = input.begin(); it; ++it) {
    bool isPointerEvent = false;
    float2 pointerCoords;
    switch (it->type) {
    case SDL_EVENT_MOUSE_MOTION:
      pointerCoords = float2(it->motion.x, it->motion.y);
      isPointerEvent = true;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
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

  float uiToWorldScale = 1.0f / virtualPointScale;

  lastFocusedPanel = focusedPanel;

  // Reset focused panel when the cursor is moved
  bool haveAnyPointerEvents = pointerInputs.size() > 0;
  if (haveAnyPointerEvents)
    focusedPanel.reset();

  for (auto &evt : pointerInputs) {
    for (size_t panelIndex = 0; panelIndex < panels.size(); panelIndex++) {
      auto &panel = panels[panelIndex];

      auto geom = panel->getGeometry().scaled(uiToWorldScale);

      float3 planePoint = geom.center;
      float3 planeNormal = linalg::cross(geom.right, geom.up);

      float dist{};
      if (gfx::intersectPlane(evt.ray.origin, evt.ray.direction, planePoint, planeNormal, dist) && dist < evt.hitDistance) {

        float3 hitPoint = evt.ray.origin + evt.ray.direction * dist;

        float3 topLeft = geom.getTopLeft();
        float fX = linalg::dot(hitPoint, geom.right) - linalg::dot(topLeft, geom.right);
        float fY = -(linalg::dot(hitPoint, geom.up) - linalg::dot(topLeft, geom.up));
        if (fX >= 0.0f && fX <= geom.size.x) {
          if (fY >= 0.0f && fY <= geom.size.y) {
            evt.hitDistance = dist;
            evt.panelCoord = float2(fX, fY);
            focusedPanel = evt.hitPanel = panel;
          }
        }
      }
    }
  }

  // Update last pointer input when new input events are received
  if (pointerInputs.size() > 0) {
    lastPointerInput.reset();
    for (auto &evt : pointerInputs) {
      if (evt.hitPanel) {
        if (!lastPointerInput || lastPointerInput->hitDistance > evt.hitDistance) {
          lastPointerInput = evt;
        }
      }
    }
  }
}

void Context::renderDebug(gfx::ShapeRenderer &sr) {
  float uiToWorldScale = 1.0f / virtualPointScale;

  for (const auto &panel : panels) {
    auto geom = panel->getGeometry().scaled(uiToWorldScale);
    float4 c = panel == focusedPanel ? float4(0, 1, 0, 1) : float4(0.8, 0.8, 0.8, 1.0);
    sr.addRect(geom.center, geom.right, geom.up, geom.size, c, 2);
  }

  bool pointerEventVisualized = false;
  auto visualizePointerInput = [&](const PointerInput &pointerInput, const float4 &color = float4(1.0f, 1.0f, 1.0f, 1.0f)) {
    if (pointerInput.hitPanel) {
      auto geom = pointerInput.hitPanel->getGeometry().scaled(uiToWorldScale);
      float3 hitCoord = geom.getTopLeft() + pointerInput.panelCoord.x * geom.right - pointerInput.panelCoord.y * geom.up;
      sr.addPoint(hitCoord, color, 5);
      pointerEventVisualized = true;
    }
  };

  // Visulize new events
  for (const auto &pointerInput : pointerInputs) {
    visualizePointerInput(pointerInput);
  }

  // Vizualize last event if no new input events were generatedz
  if (lastPointerInput && !pointerEventVisualized) {
    visualizePointerInput(lastPointerInput.value(), float4(0.5f, 0.5f, 0.5f, 1.0f));
  }
}

struct RenderContext {
  PanelPtr panel;
  gfx::DrawQueuePtr queue;
  std::shared_ptr<ContextCachedPanel> cached;
  float scaling;
  float pixelsPerPoint;
};

void Context::evaluate(gfx::DrawQueuePtr queue, double time, float deltaTime) {
  bool panelFocusChanged = lastFocusedPanel != focusedPanel;

  for (auto &panel : panels) {
    eguiInputTranslator.begin(time, deltaTime);

    if (panelFocusChanged && lastFocusedPanel == panel) {
      egui::InputEvent evt;
      auto &pointerGone = evt.pointerGone;
      pointerGone.type = egui::InputEventType::PointerGone;
      eguiInputTranslator.pushEvent(evt);

      // Simulate deselect by simulating up/down for all buttons
      auto &pointerButton = evt.pointerButton;
      for (auto &button : magic_enum::enum_values<egui::PointerButton>()) {
        pointerButton.type = egui::InputEventType::PointerButton;
        pointerButton.button = button;
        pointerButton.pos = egui::toPos2(float2(-1, -1));
        pointerButton.type = egui::InputEventType::PointerButton;
        pointerButton.pressed = true;
        eguiInputTranslator.pushEvent(evt);
        pointerButton.pressed = false;
        eguiInputTranslator.pushEvent(evt);
      }
    }

    for (auto &pointerInput : pointerInputs) {
      if (pointerInput.hitPanel == panel) {
        const SDL_Event &sdlEvent = *pointerInput.iterator;
        switch (sdlEvent.type) {
        case SDL_EVENT_MOUSE_MOTION: {
          egui::InputEvent evt;
          auto &oevent = evt.pointerMoved;
          oevent.type = egui::InputEventType::PointerMoved;
          oevent.pos = egui::toPos2(pointerInput.panelCoord * virtualPointScale);
          eguiInputTranslator.pushEvent(evt);
          break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
          egui::InputEvent evt;
          auto &ievent = sdlEvent.button;
          auto &oevent = evt.pointerButton;
          oevent.type = egui::InputEventType::PointerButton;
          oevent.pos = egui::toPos2(pointerInput.panelCoord * virtualPointScale);
          oevent.pressed = sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
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
      // for (auto &otherEvent : otherEvents) {
        // eguiInputTranslator.translateEvent(*otherEvent);
      // }
    }
    eguiInputTranslator.end();
    renderPanel(queue, panel, *eguiInputTranslator.getOutput());
  }
}

// Check size of panel in view to determine resolution to render at
float Context::computeRenderResolution(PanelPtr panel) const {
  PanelGeometry uiGeometry = panel->getGeometry();
  PanelGeometry worldGeometry = uiGeometry.scaled(1.0f / virtualPointScale);

  auto project = [&](float3 v) {
    float4 tmp = linalg::mul(sizedView.viewProj, float4(v, 1.0f));
    return ((tmp.xyz() / tmp.w) * float3(0.5f) + float3(0.5f)) * float3(sizedView.size, 1.0f);
  };

  float3 projTL = project(worldGeometry.getTopLeft());
  float3 projTR = project(worldGeometry.getTopLeft() + worldGeometry.right * worldGeometry.size.x);
  float sizeX = linalg::distance(projTL, projTR);

  float res = sizeX / uiGeometry.size.x;
  res = std::round(res / resolutionGranularity) * resolutionGranularity;
  return linalg::clamp(res, minResolution, maxResolution);
}

void Context::renderPanel(gfx::DrawQueuePtr queue, PanelPtr panel, egui::Input input) {
  auto geometry = panel->getGeometry();

  input.screenRect.min = egui::Pos2{};
  input.screenRect.max = egui::toPos2(geometry.size);

  input.pixelsPerPoint = computeRenderResolution(panel);

  const egui::FullOutput &output = panel->render(input);

  std::shared_ptr<ContextCachedPanel> cachedPanel = getCachedPanel(panel);
  EguiRenderer &renderer = cachedPanel->renderer;

  float uiToWorldScale = 1.0f / virtualPointScale;

  float4x4 rootTransform = linalg::identity;
  PanelGeometry geom = panel->getGeometry().scaled(uiToWorldScale);

  // Geometry comes in in UI coordinates, and it's also scaled by pixelsPerPoint passed to egui
  //  so it needs to be scaled by `1.0f / (scale * pixelsPerPoint)`
  rootTransform = linalg::mul(linalg::scaling_matrix(float3(1.0f / (virtualPointScale * input.pixelsPerPoint))), rootTransform);

  // Derive rotation matrix from panel right/up
  float4x4 rotationMat;
  rotationMat.x = float4(geom.right, 0);
  rotationMat.y = float4(-geom.up, 0); // Render UI as using Y+ is down
  rotationMat.z = float4(linalg::cross(geom.right, geom.up), 0);
  rotationMat.w = float4(0, 0, 0, 1);
  rootTransform = linalg::mul(rotationMat, rootTransform);

  // Place drawable at top-left corner of panel UI
  rootTransform = linalg::mul(linalg::translation_matrix(geom.getTopLeft()), rootTransform);

  // NOTE: clipping disabled since we're rendering in world space
  // TODO(guusw): geometry needs to be clipped using stencil or rendered to texture first
  renderer.render(output, rootTransform, queue, false);
}

std::shared_ptr<ContextCachedPanel> Context::getCachedPanel(PanelPtr panel) {
  auto it = cachedPanels.find(panel.get());
  if (it == cachedPanels.end()) {
    it = cachedPanels.insert(std::make_pair(panel.get(), std::make_shared<ContextCachedPanel>())).first;
  }
  return it->second;
}

} // namespace shards::spatial
