#include "../../core/platform.hpp"
#include "egui_types.hpp"
#include "input.hpp"
#include <gfx/linalg.hpp>
#include <SDL.h>
#include <SDL_keycode.h>
#include <gfx/window.hpp>
#include <gfx/types.hpp>
#include "input/events.hpp"
#include "input/state.hpp"
#include "renderer.hpp"
#include <map>

namespace gfx {

struct SDLCursor {
  SDL_Cursor *cursor{};
  SDLCursor(SDL_SystemCursor id) { cursor = SDL_CreateSystemCursor(id); }
  SDLCursor(SDLCursor &&rhs) {
    cursor = rhs.cursor;
    rhs.cursor = nullptr;
  }
  SDLCursor(const SDLCursor &) = delete;
  SDLCursor &operator=(const SDLCursor &) = delete;
  SDLCursor &operator=(SDLCursor &&rhs) {
    cursor = rhs.cursor;
    rhs.cursor = nullptr;
    return *this;
  }
  ~SDLCursor() {
    if (cursor)
      SDL_FreeCursor(cursor);
  }
  operator SDL_Cursor *() const { return cursor; }
};

struct CursorMap {
  std::map<egui::CursorIcon, SDL_SystemCursor> cursorMap{};

  CursorMap() {
    cursorMap.insert_or_assign(egui::CursorIcon::Text, SDL_SystemCursor::SDL_SYSTEM_CURSOR_IBEAM);
    cursorMap.insert_or_assign(egui::CursorIcon::PointingHand, SDL_SystemCursor::SDL_SYSTEM_CURSOR_HAND);
    cursorMap.insert_or_assign(egui::CursorIcon::Grab, SDL_SystemCursor::SDL_SYSTEM_CURSOR_HAND);
    cursorMap.insert_or_assign(egui::CursorIcon::Grabbing, SDL_SystemCursor::SDL_SYSTEM_CURSOR_HAND);
    cursorMap.insert_or_assign(egui::CursorIcon::NoDrop, SDL_SystemCursor::SDL_SYSTEM_CURSOR_NO);
    cursorMap.insert_or_assign(egui::CursorIcon::NotAllowed, SDL_SystemCursor::SDL_SYSTEM_CURSOR_NO);
    cursorMap.insert_or_assign(egui::CursorIcon::Crosshair, SDL_SystemCursor::SDL_SYSTEM_CURSOR_CROSSHAIR);
    cursorMap.insert_or_assign(egui::CursorIcon::ResizeNeSw, SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENESW);
    cursorMap.insert_or_assign(egui::CursorIcon::ResizeNwSe, SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENWSE);
    cursorMap.insert_or_assign(egui::CursorIcon::Default, SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW);
    cursorMap.insert_or_assign(egui::CursorIcon::ResizeVertical, SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENS);
    cursorMap.insert_or_assign(egui::CursorIcon::ResizeHorizontal, SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZEWE);
  }

  SDL_SystemCursor *getCursor(egui::CursorIcon cursor) {
    auto it = cursorMap.find(cursor);
    if (it == cursorMap.end())
      return nullptr;
    return &it->second;
  }

  static CursorMap &getInstance() {
    static CursorMap map;
    return map;
  }
};

static egui::ModifierKeys translateModifierKeys(SDL_Keymod flags) {
  return egui::ModifierKeys{
      .alt = (flags & KMOD_ALT) != 0,
      .ctrl = (flags & KMOD_CTRL) != 0,
      .shift = (flags & KMOD_SHIFT) != 0,
      .macCmd = (flags & KMOD_GUI) != 0,
      .command = (flags & KMOD_GUI) != 0,
  };
}

void EguiInputTranslator::setupInputRegion(const shards::input::InputRegion &region, const int4 &mappedWindowRegion) {
  // UI Points per pixel
  float eguiDrawScale = region.uiScalingFactor;

  // Drawable/Window scale
  float2 inputScale = float2(region.pixelSize) / float2(region.size);
  windowToEguiScale = inputScale / eguiDrawScale;

  // Convert from pixel to window coordinatesmapping
  this->mappedWindowRegion = float4(mappedWindowRegion) / float4(inputScale.x, inputScale.y, inputScale.x, inputScale.y);

  int2 mappedRegionSize{float2(gfx::Rect::fromCorners(mappedWindowRegion).getSize()) / inputScale};
  float2 eguiScreenSize = float2(mappedRegionSize) * windowToEguiScale;

  // Convert mapped window region to render area
  input.screenRect = egui::Rect{
      .min = egui::Pos2{0, 0},
      .max = egui::Pos2{eguiScreenSize.x, eguiScreenSize.y},
  };
  input.pixelsPerPoint = eguiDrawScale;
}

void EguiInputTranslator::begin(double time, float deltaTime) {
  reset();

  egui::ModifierKeys modifierKeys{};

  input.time = time;
  input.predictedDeltaTime = deltaTime;
  input.modifierKeys = modifierKeys;
}

egui::PointerButton translateMouseButton(uint32_t button) {
  switch (button) {
  case SDL_BUTTON_LEFT:
    return egui::PointerButton::Primary;
    break;
  case SDL_BUTTON_MIDDLE:
    return egui::PointerButton::Middle;
    break;
  case SDL_BUTTON_RIGHT:
    return egui::PointerButton::Secondary;
    break;
  default:
    throw std::out_of_range("SDL Mouse Button");
  }
}

bool EguiInputTranslator::translateEvent(const EguiInputTranslatorArgs &args, const shards::input::ConsumableEvent &event) {
  using egui::InputEvent;
  using egui::InputEventType;

  bool handled = false;
  auto newEvent = [&](egui::InputEventType type) -> InputEvent & {
    InputEvent &event = events.emplace_back();
    event.common.type = type;
    handled = true;
    return event;
  };

  auto updateCursorPosition = [&](int32_t x, int32_t y) -> const egui::Pos2 & {
    float2 cursorPosition = float2(x, y);
    return lastCursorPosition = egui::Pos2{.x = cursorPosition.x, .y = cursorPosition.y};
  };

  using namespace shards::input;
  std::visit(
      [&](auto &arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, PointerTouchEvent>) {
          // if (arg.pressed && (!args.canReceiveInput || event.isConsumed()))
          //   return;
          // auto &oevent = newEvent(InputEventType::Touch).touch;
          // oevent.pos = egui::Pos2{arg.pos.x, arg.pos.y};
          // oevent.deviceId = egui::TouchDeviceId{1};
          // oevent.id = egui::TouchId{uint64_t(arg.index)};
          // oevent.phase = arg.pressed ? egui::TouchPhase::Start : egui::TouchPhase::End;
          // oevent.force = arg.pressure;
        } else if constexpr (std::is_same_v<T, PointerTouchMoveEvent>) {
          // if (!args.canReceiveInput || event.isConsumed())
          //   return;
          // auto &oevent = newEvent(InputEventType::Touch).touch;
          // oevent.pos = egui::Pos2{arg.pos.x, arg.pos.y};
          // oevent.deviceId = egui::TouchDeviceId{1};
          // oevent.id = egui::TouchId{uint64_t(arg.index)};
          // oevent.phase = egui::TouchPhase::Move;
          // oevent.force = arg.pressure;
        } else if constexpr (std::is_same_v<T, PointerMoveEvent>) {
          if (!args.canReceiveInput || event.isConsumed())
            return;
          auto &oevent = newEvent(InputEventType::PointerMoved).pointerMoved;
          oevent.pos = translatePointerPos(updateCursorPosition(arg.pos.x, arg.pos.y));
        } else if constexpr (std::is_same_v<T, PointerButtonEvent>) {
          if ((args.canReceiveInput && !event.isConsumed()) || !arg.pressed) {
            auto &oevent = newEvent(InputEventType::PointerButton).pointerButton;
            oevent.pos = translatePointerPos(updateCursorPosition(arg.pos.x, arg.pos.y));
            oevent.button = translateMouseButton(arg.index);
            oevent.pressed = arg.pressed;
            oevent.modifiers = translateModifierKeys(arg.modifiers);
          }
          if constexpr (!shards::input::Pointer::HasPersistentPointer) {
            if (!arg.pressed) {
              (void)newEvent(InputEventType::PointerGone).pointerGone;
            }
          }
        } else if constexpr (std::is_same_v<T, ScrollEvent>) {
          if (!args.canReceiveInput || event.isConsumed())
            return;
          auto &oevent = newEvent(InputEventType::Scroll).scroll;
          oevent.delta = egui::Pos2{0.0f, arg.delta};
        } else if constexpr (std::is_same_v<T, KeyEvent>) {
          if ((args.canReceiveInput && !event.isConsumed()) || !arg.pressed) {
            auto &oevent = newEvent(InputEventType::Key).key;
            oevent.key = SDL_KeyCode(arg.key);
            oevent.pressed = arg.pressed;
            oevent.modifiers = translateModifierKeys(arg.modifiers);
            oevent.repeat = arg.repeat > 0;
            if (arg.repeat) {
              oevent.repeat = true;
            }

            if (arg.pressed) {
              if ((arg.modifiers & KMOD_PRIMARY) && arg.key == SDLK_c) {
                newEvent(InputEventType::Copy);
              } else if ((arg.modifiers & KMOD_PRIMARY) && arg.key == SDLK_v) {
                auto &evt = newEvent(InputEventType::Paste);
                auto clipboardPtr = SDL_GetClipboardText();
                evt.paste.str = strings.emplace_back(clipboardPtr).c_str();
                SDL_free(clipboardPtr);
              } else if ((arg.modifiers & KMOD_PRIMARY) && arg.key == SDLK_x) {
                newEvent(InputEventType::Cut);
              }
            }
          }
        } else if constexpr (std::is_same_v<T, TextEvent>) {
          if (!args.canReceiveInput || event.isConsumed())
            return;
          if (imeComposing) {
            imeComposing = false;
          }
          auto &oevent = newEvent(InputEventType::Text).text;
          oevent.text = arg.text.c_str();
        } else if constexpr (std::is_same_v<T, TextCompositionEvent>) {
          if (!args.canReceiveInput || event.isConsumed())
            return;
          if (!imeComposing) {
            (void)newEvent(InputEventType::CompositionStart).compositionStart;
            imeComposing = true;
          }
          if (!arg.text.empty()) {
            auto &oevent = newEvent(InputEventType::CompositionUpdate).text;
            oevent.text = arg.text.c_str();
          }
        } else if constexpr (std::is_same_v<T, TextCompositionEndEvent>) {
          // Check if it's safe to ignore checks here
          // if (!args.canReceiveInput || event.isConsumed())
          //   return;
          auto &oevent = newEvent(InputEventType::CompositionEnd).text;
          oevent.text = arg.text.c_str();
          imeComposing = false;
        }
      },
      event.event);
  return handled;
}

void EguiInputTranslator::end() {
  input.inputEvents = events.data();
  input.numInputEvents = events.size();

  input.numDroppedFiles = 0;
  input.numHoveredFiles = 0;
}

const egui::Input *EguiInputTranslator::translateFromInputEvents(const EguiInputTranslatorArgs &args) {
  begin(args.time, args.deltaTime);
  setupInputRegion(args.region, args.mappedWindowRegion);
  for (const auto &event : args.events)
    translateEvent(args, event);
  end();
  return getOutput();
}

const egui::Input *EguiInputTranslator::getOutput() { return &input; }

egui::Pos2 EguiInputTranslator::translatePointerPos(const egui::Pos2 &pos) {
  return egui::Pos2{
      (pos.x - mappedWindowRegion.x) * windowToEguiScale.x,
      (pos.y - mappedWindowRegion.y) * windowToEguiScale.y,
  };
}

void EguiInputTranslator::applyOutput(const egui::IOOutput &output) {
  updateTextCursorPosition(output.validCursorPosition ? &output.textCursorPosition : nullptr);

  if (output.copiedText)
    copyText(output.copiedText);

  if (input.numInputEvents > 0)
    updateCursorIcon(output.cursorIcon);
}

void EguiInputTranslator::updateTextCursorPosition(const egui::Pos2 *pos) {
  if (pos) {
    SDL_Rect rect;
    rect.x = int(pos->x);
    rect.y = int(pos->y);
    rect.w = 20;
    rect.h = 20;

    if (!textInputActive) {
      outputMessages.push_back(shards::input::BeginTextInputMessage{
          .inputRect = rect,
      });
      textInputActive = true;
    }
  } else {
    if (textInputActive) {
      outputMessages.push_back(shards::input::EndTextInputMessage{});
      textInputActive = false;
      imeComposing = false;
    }
  }
}

void EguiInputTranslator::copyText(const char *text) { SDL_SetClipboardText(text); }

void EguiInputTranslator::updateCursorIcon(egui::CursorIcon icon) {
  if (icon == egui::CursorIcon::None) {
    outputMessages.push_back(shards::input::SetCursorMessage{
        .visible = false,
    });
  } else {
    auto *cursor = CursorMap::getInstance().getCursor(icon);
    outputMessages.push_back(shards::input::SetCursorMessage{
        .visible = true,
        .cursor = cursor ? *cursor : SDL_SYSTEM_CURSOR_ARROW,
    });
  }
}

void EguiInputTranslator::reset() {
  strings.clear();
  events.clear();
  outputMessages.clear();
}

EguiInputTranslator *EguiInputTranslator::create() { return new EguiInputTranslator(); }

void EguiInputTranslator::destroy(EguiInputTranslator *obj) { delete obj; }

} // namespace gfx
