#include "input.hpp"
#include <SDL3/SDL_events.h>

namespace shards::input {
ConsumeEventFilter categorizeEvent(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_JOYSTICK_AXIS_MOTION:
  case SDL_EVENT_JOYSTICK_HAT_MOTION:
  case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
  case SDL_EVENT_JOYSTICK_BUTTON_UP:
  case SDL_EVENT_JOYSTICK_ADDED:
  case SDL_EVENT_JOYSTICK_REMOVED:
  case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
  case SDL_EVENT_GAMEPAD_BUTTON_UP:
  case SDL_EVENT_GAMEPAD_ADDED:
  case SDL_EVENT_GAMEPAD_REMOVED:
  case SDL_EVENT_GAMEPAD_REMAPPED:
  case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
  case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
  case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
  case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
    return ConsumeEventFilter::Controller;
  case SDL_EVENT_FINGER_DOWN:
  case SDL_EVENT_FINGER_UP:
  case SDL_EVENT_FINGER_MOTION:
    return ConsumeEventFilter::Touch;
  case SDL_EVENT_MOUSE_WHEEL:
  case SDL_EVENT_MOUSE_BUTTON_UP:
  case SDL_EVENT_MOUSE_MOTION:
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    return ConsumeEventFilter::Mouse;
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
  case SDL_EVENT_TEXT_EDITING:
  case SDL_EVENT_TEXT_INPUT:
  case SDL_EVENT_KEYMAP_CHANGED:
  case SDL_EVENT_TEXT_EDITING_EXT:
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
