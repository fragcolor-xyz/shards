#include "input.hpp"

namespace shards::input {
void InputBuffer::consumeEvents(ConsumeEventFilter filter, void *by) {
  for (auto it = begin(); it; ++it) {
    switch (it->type) {
    case SDL_JOYAXISMOTION:
    case SDL_JOYBALLMOTION:
    case SDL_JOYHATMOTION:
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
    case SDL_JOYDEVICEADDED:
    case SDL_JOYDEVICEREMOVED:
    case SDL_JOYBATTERYUPDATED:
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
      if ((filter & ConsumeEventFilter::Controller) != ConsumeEventFilter::None)
        it.consume(by);
      break;
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEWHEEL:
    case SDL_DOLLARGESTURE:
    case SDL_DOLLARRECORD:
    case SDL_MULTIGESTURE:
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
      if ((filter & ConsumeEventFilter::PointerDown) != ConsumeEventFilter::None)
        it.consume(by);
      break;
    case SDL_FINGERUP:
    case SDL_MOUSEBUTTONUP:
      if ((filter & ConsumeEventFilter::PointerUp) != ConsumeEventFilter::None)
        it.consume(by);
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    case SDL_TEXTEDITING:
    case SDL_TEXTINPUT:
    case SDL_KEYMAPCHANGED:
    case SDL_TEXTEDITING_EXT:
      if ((filter & ConsumeEventFilter::Keyboard) != ConsumeEventFilter::None)
        it.consume(by);
      break;
    }
  }
}
} // namespace shards::input