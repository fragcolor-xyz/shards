#include "input.hpp"
#include "../linalg.hpp"
#include <SDL_events.h>
#include <gfx/window.hpp>
#include "renderer.hpp"

namespace gfx {
const egui::Input *EguiInputTranslator::translateFromInputEvents(const std::vector<SDL_Event> &sdlEvents, Window &window,
                                                                 double time, float deltaTime) {
  using egui::InputEvent;
  using egui::InputEventType;

  float drawScale = EguiRenderer::getDrawScale(window);
  float2 screenSize = window.getDrawableSize() / drawScale;

  input.screenRect = egui::Rect{
      .min = egui::Pos2{0, 0},
      .max = egui::Pos2{screenSize.x, screenSize.y},
  };
  input.pixelsPerPoint = drawScale;

  events.clear();
  auto newEvent = [&](egui::InputEventType type) -> InputEvent & {
    InputEvent &event = events.emplace_back();
    event.common.type = type;
    return event;
  };

  SDL_Keymod sdlModifierKeys = SDL_GetModState();
  egui::ModifierKeys modifierKeys{
      .alt = (sdlModifierKeys & KMOD_ALT) != 0,
      .ctrl = (sdlModifierKeys & KMOD_CTRL) != 0,
      .shift = (sdlModifierKeys & KMOD_SHIFT) != 0,
      .macCmd = (sdlModifierKeys & KMOD_GUI) != 0,
      .command = (sdlModifierKeys & KMOD_GUI) != 0,
  };

  auto updateCursorPosition = [&](int32_t x, int32_t y) -> const egui::Pos2 & {
    float2 virtualCursorPosition = float2(x, y) / drawScale;
    return lastCursorPosition = egui::Pos2{.x = virtualCursorPosition.x, .y = virtualCursorPosition.y};
  };

  for (auto &sdlEvent : sdlEvents) {
    switch (sdlEvent.type) {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
      auto &ievent = sdlEvent.button;
      auto &oevent = newEvent(InputEventType::PointerButton).pointerButton;
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
        // ignore this button
        events.pop_back();
        continue;
        break;
      }
      oevent.pressed = ievent.type == SDL_MOUSEBUTTONDOWN;
      oevent.modifiers = modifierKeys;
      oevent.pos = updateCursorPosition(ievent.x, ievent.y);
    }
    case SDL_MOUSEMOTION: {
      auto &ievent = sdlEvent.motion;
      auto &oevent = newEvent(InputEventType::PointerMoved).pointerMoved;
      oevent.pos = updateCursorPosition(ievent.x, ievent.y);
      break;
    }
    case SDL_MOUSEWHEEL: {
      auto &ievent = sdlEvent.wheel;
      auto &oevent = newEvent(InputEventType::Scroll).scroll;
      oevent.delta = egui::Pos2{
          .x = float(ievent.preciseX),
          .y = float(ievent.preciseY),
      };
      break;
    }
    }
  }

  input.inputEvents = events.data();
  input.numInputEvents = events.size();

  input.numDroppedFiles = 0;
  input.numHoveredFiles = 0;

  input.time = time;
  input.predictedDeltaTime = deltaTime;
  input.modifierKeys = modifierKeys;

  return &input;
}

EguiInputTranslator *EguiInputTranslator::create() { return new EguiInputTranslator(); }
void EguiInputTranslator::destroy(EguiInputTranslator *obj) { delete obj; }

} // namespace gfx