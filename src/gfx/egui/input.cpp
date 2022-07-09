#include "input.hpp"
#include "../linalg.hpp"
#include <SDL.h>
#include <gfx/window.hpp>
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
  std::map<egui::CursorIcon, SDLCursor> cursorMap{};

  CursorMap() {
    cursorMap.insert_or_assign(egui::CursorIcon::Text, SDLCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_IBEAM));
    cursorMap.insert_or_assign(egui::CursorIcon::PointingHand, SDLCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_HAND));
    cursorMap.insert_or_assign(egui::CursorIcon::Crosshair, SDLCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_CROSSHAIR));
    cursorMap.insert_or_assign(egui::CursorIcon::ResizeNeSw, SDLCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENESW));
    cursorMap.insert_or_assign(egui::CursorIcon::ResizeNwSe, SDLCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENWSE));
    cursorMap.insert_or_assign(egui::CursorIcon::Default, SDLCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW));
    cursorMap.insert_or_assign(egui::CursorIcon::ResizeVertical, SDLCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENS));
    cursorMap.insert_or_assign(egui::CursorIcon::ResizeHorizontal, SDLCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZEWE));
  }

  SDL_Cursor *getCursor(egui::CursorIcon cursor) {
    auto it = cursorMap.find(cursor);
    if (it == cursorMap.end())
      return nullptr;
    return it->second;
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

const egui::Input *EguiInputTranslator::translateFromInputEvents(const std::vector<SDL_Event> &sdlEvents, Window &window,
                                                                 double time, float deltaTime) {
  using egui::InputEvent;
  using egui::InputEventType;

  reset();

  float eguiDrawScale = EguiRenderer::getDrawScale(window);
  float2 eguiScreenSize = window.getVirtualDrawableSize();

  input.screenRect = egui::Rect{
      .min = egui::Pos2{0, 0},
      .max = egui::Pos2{eguiScreenSize.x, eguiScreenSize.y},
  };
  input.pixelsPerPoint = eguiDrawScale;

  auto newEvent = [&](egui::InputEventType type) -> InputEvent & {
    InputEvent &event = events.emplace_back();
    event.common.type = type;
    return event;
  };

  egui::ModifierKeys modifierKeys = translateModifierKeys(SDL_GetModState());

  auto updateCursorPosition = [&](int32_t x, int32_t y) -> const egui::Pos2 & {
    float2 cursorPosition = float2(x, y);
    if (!window.isWindowSizeVirtual())
      cursorPosition = window.toVirtualCoord(cursorPosition);

    return lastCursorPosition = egui::Pos2{.x = cursorPosition.x, .y = cursorPosition.y};
  };

  std::vector<std::function<void()>> deferQueue;
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
    case SDL_TEXTEDITING: {
      auto &ievent = sdlEvent.edit;

      std::string editingText = ievent.text;
      if (!imeComposing) {
        imeComposing = true;
        newEvent(InputEventType::CompositionStart);
      }

      newEvent(InputEventType::CompositionUpdate);
      strings.emplace_back(ievent.text);
      deferQueue.emplace_back([this, eventIdx = events.size() - 1, stringIdx = strings.size() - 1]() {
        events[eventIdx].compositionUpdate.text = strings[stringIdx].c_str();
      });
      break;
    }
    case SDL_TEXTINPUT: {
      auto &ievent = sdlEvent.text;

      if (imeComposing) {
        newEvent(InputEventType::CompositionEnd);
        strings.emplace_back(ievent.text);
        deferQueue.emplace_back([this, eventIdx = events.size() - 1, stringIdx = strings.size() - 1]() {
          events[eventIdx].compositionEnd.text = strings[stringIdx].c_str();
        });
        imeComposing = false;
      } else {
        newEvent(InputEventType::Text);
        strings.emplace_back(ievent.text);

        deferQueue.emplace_back([this, eventIdx = events.size() - 1, stringIdx = strings.size() - 1]() {
          events[eventIdx].text.text = strings[stringIdx].c_str();
        });
      }
      break;
    }
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      auto &ievent = sdlEvent.key;
      auto &oevent = newEvent(InputEventType::Key).key;
      oevent.key = SDL_KeyCode(ievent.keysym.sym);
      oevent.pressed = ievent.type == SDL_KEYDOWN;
      oevent.modifiers = translateModifierKeys(SDL_Keymod(ievent.keysym.mod));

      // Translate cut/copy/paste using the standard keys combos
      if (ievent.type == SDL_KEYDOWN) {
        if ((ievent.keysym.mod & KMOD_CTRL) && ievent.keysym.sym == SDLK_c) {
          newEvent(InputEventType::Copy);
        } else if ((ievent.keysym.mod & KMOD_CTRL) && ievent.keysym.sym == SDLK_v) {
          newEvent(InputEventType::Paste);

          strings.emplace_back(SDL_GetClipboardText());
          deferQueue.emplace_back([this, eventIdx = events.size() - 1, stringIdx = strings.size() - 1]() {
            events[eventIdx].paste.str = strings[stringIdx].c_str();
          });
        } else if ((ievent.keysym.mod & KMOD_CTRL) && ievent.keysym.sym == SDLK_x) {
          newEvent(InputEventType::Cut);
        }
      }
      break;
    }
    }
  }

  for (auto &deferred : deferQueue) {
    deferred();
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

void EguiInputTranslator::updateTextCursorPosition(Window &window, const egui::Pos2 *pos) {
  if (pos) {
    float2 drawScale = window.getDrawScale();
    SDL_Rect rect;
    rect.x = int(pos->x * drawScale.x);
    rect.y = int(pos->y * drawScale.y);
    rect.w = 500;
    rect.h = 80;
    SDL_SetTextInputRect(&rect);

    if (!textInputActive) {
      SDL_StartTextInput();
      textInputActive = true;
    }
  } else {
    if (textInputActive) {
      SDL_StopTextInput();
      textInputActive = false;
      imeComposing = false;
    }
  }
}

void EguiInputTranslator::copyText(const char *text) { SDL_SetClipboardText(text); }

void EguiInputTranslator::updateCursorIcon(egui::CursorIcon icon) {
  if (icon == egui::CursorIcon::None) {
    SDL_ShowCursor(SDL_DISABLE);
  } else {
    SDL_ShowCursor(SDL_ENABLE);
    SDL_Cursor *cursor = CursorMap::getInstance().getCursor(icon);
    SDL_SetCursor(cursor);
  }
}

void EguiInputTranslator::reset() {
  strings.clear();
  events.clear();
}

EguiInputTranslator *EguiInputTranslator::create() { return new EguiInputTranslator(); }

void EguiInputTranslator::destroy(EguiInputTranslator *obj) { delete obj; }

} // namespace gfx
