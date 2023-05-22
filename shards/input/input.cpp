#include "input.hpp"

namespace shards::input {
ConsumeEventFilter categorizeEvent(const SDL_Event &event) {
  switch (event.type) {
  case SDL_JOYAXISMOTION:
  case SDL_JOYBALLMOTION:
  case SDL_JOYHATMOTION:
  case SDL_JOYBUTTONDOWN:
  case SDL_JOYBUTTONUP:
  case SDL_JOYDEVICEADDED:
  case SDL_JOYDEVICEREMOVED:
  case SDL_CONTROLLERAXISMOTION:
  case SDL_CONTROLLERBUTTONDOWN:
  case SDL_CONTROLLERBUTTONUP:
  case SDL_CONTROLLERDEVICEADDED:
  case SDL_CONTROLLERDEVICEREMOVED:
  case SDL_CONTROLLERDEVICEREMAPPED:
  case SDL_CONTROLLERTOUCHPADDOWN:
  case SDL_CONTROLLERTOUCHPADMOTION:
  case SDL_CONTROLLERTOUCHPADUP:
  case SDL_CONTROLLERSENSORUPDATE:
    return ConsumeEventFilter::Controller;
  case SDL_DOLLARGESTURE:
  case SDL_DOLLARRECORD:
  case SDL_MULTIGESTURE:
  case SDL_FINGERDOWN:
  case SDL_FINGERMOTION:
  case SDL_FINGERUP:
    return ConsumeEventFilter::Touch;
  case SDL_MOUSEWHEEL:
  case SDL_MOUSEBUTTONUP:
  case SDL_MOUSEMOTION:
  case SDL_MOUSEBUTTONDOWN:
    return ConsumeEventFilter::Mouse;
  case SDL_KEYDOWN:
  case SDL_KEYUP:
  case SDL_TEXTEDITING:
  case SDL_TEXTINPUT:
  case SDL_KEYMAPCHANGED:
  case SDL_TEXTEDITING_EXT:
    return ConsumeEventFilter::Keyboard;
  default:
    return ConsumeEventFilter::None;
  }
}

void InputBuffer::consumeEvents(ConsumeEventFilter filter, void *by) {
  for (auto it = begin(); it; ++it) {
    ConsumeEventFilter type = categorizeEvent(*it);
    if ((filter & type) != ConsumeEventFilter::None)
      it.consume(by);
  }
}
} // namespace shards::input
