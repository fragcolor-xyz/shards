#ifndef AAC121E8_D624_4AD0_81F6_587EB5F716DC
#define AAC121E8_D624_4AD0_81F6_587EB5F716DC
#include "input.hpp"
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <boost/container/flat_set.hpp>

namespace shards::input {

static inline constexpr int NumMouseButtons = 3;

// Wraps input captured at a specific time
struct InputState {
  boost::container::flat_set<SDL_Keycode> heldKeys;
  SDL_Keymod modifiers{};
  uint32_t mouseButtonState{};
  float2 cursorPosition;
  InputRegion region;
  bool suspended{};

  bool isKeyHeld(SDL_Keycode keycode) const { return heldKeys.contains(keycode); }
  bool isMouseButtonHeld(int buttonIndex) const { return (SDL_BUTTON(buttonIndex) & mouseButtonState) != 0; }

  void reset() {
    mouseButtonState = 0;
    modifiers = SDL_Keymod(0);
    cursorPosition = float2{};
    heldKeys.clear();
  }

  void update() { // Retrieve up-to-date keyboard state
    int numKeys{};
    auto keyStates = SDL_GetKeyboardState(&numKeys);
    heldKeys.clear();
    for (int i = 0; i < numKeys; i++) {
      SDL_Keycode code = SDL_GetKeyFromScancode((SDL_Scancode)i);
      if (keyStates[i] == 1) {
        heldKeys.insert(code);
      }
    }

    // Retrieve mouse button state
    int2 cursorPositionInt{};
    mouseButtonState = SDL_GetMouseState(&cursorPositionInt.x, &cursorPositionInt.y);
    cursorPosition = float2(cursorPositionInt);

    // Update key mod state
    modifiers = SDL_GetModState();
  }
};
} // namespace shards::input

#endif /* AAC121E8_D624_4AD0_81F6_587EB5F716DC */
