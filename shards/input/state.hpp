#ifndef AAC121E8_D624_4AD0_81F6_587EB5F716DC
#define AAC121E8_D624_4AD0_81F6_587EB5F716DC
#include "input.hpp"
#include "../core/platform.hpp"
#include "sdl.hpp"
#include <functional>
#include <optional>
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>

#if !SHARDS_GFX_SDL
#include <shards/gfx/gfx_events_em.hpp>
#endif

namespace shards::input {

static inline constexpr int NumMouseButtons = 3;

struct Pointer {
  float2 position{};
  float pressure{};
  SDL_TouchID touchId;

#if SH_IOS | SH_ANDROID
  // True when a mouse-like persistent pointer is available
  static constexpr bool HasPersistentPointer = false;
#else
  // True when a mouse-like persistent pointer is available
  static constexpr bool HasPersistentPointer = true;
#endif
};

struct Pointers {
  mutable boost::container::small_flat_map<SDL_FingerID, Pointer, 32> pointers;

  std::optional<std::reference_wrapper<Pointer>> get(SDL_FingerID id) const {
    auto it = pointers.find(id);
    if (it == pointers.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  void erase(SDL_FingerID id) { pointers.erase(id); }

  // Remove all pointers not in other
  void filter(const Pointers &other) {
    for (auto it = pointers.begin(); it != pointers.end();) {
      if (!other.pointers.contains(it->first)) {
        it = pointers.erase(it);
      } else {
        ++it;
      }
    }
  }

  Pointer &getOrInsert(SDL_FingerID id) {
    auto it = pointers.find(id);
    if (it == pointers.end()) {
      it = pointers.emplace(id, Pointer{}).first;
    }
    return it->second;
  }

  size_t size() const { return pointers.size(); }

  bool contains(SDL_FingerID id) const {
    auto it = pointers.find(id);
    return it != pointers.end();
  }

  bool any() const { return !pointers.empty(); }

  float2 centroid() const {
    float2 c{};
    if (any()) {
      for (auto &[id, pointer] : pointers) {
        c += pointer.position;
      }
      c /= pointers.size();
    }
    return c;
  }
};

// Wraps input captured at a specific time
struct InputState {
  boost::container::flat_set<SDL_Keycode> heldKeys;
  SDL_Keymod modifiers{};
  uint32_t mouseButtonState{};
  float2 cursorPosition;
  InputRegion region;
  bool suspended{};

  Pointers pointers;

  bool isKeyHeld(SDL_Keycode keycode) const { return heldKeys.contains(keycode); }

  bool isMouseButtonHeld(int buttonIndex) const {
    if constexpr (Pointer::HasPersistentPointer) {
      return (SDL_BUTTON(buttonIndex) & mouseButtonState) != 0;
    } else {
      if (buttonIndex == SDL_BUTTON_LEFT) {
        return pointers.any();
      }
      return false;
    }
  }

  void reset() {
    mouseButtonState = 0;
    modifiers = SDL_Keymod(0);
    cursorPosition = float2{};
    heldKeys.clear();
  }

  void update() { // Retrieve up-to-date keyboard state
#if SHARDS_GFX_SDL
    int numKeys{};
    auto keyStates = SDL_GetKeyboardState(&numKeys);
    heldKeys.clear();
    for (int i = 0; i < numKeys; i++) {
      SDL_Keycode code = SDL_GetKeyFromScancode((SDL_Scancode)i);
      if (keyStates[i] == 1) {
        heldKeys.insert(code);
      }
    }

    if constexpr (Pointer::HasPersistentPointer) {
      // Retrieve mouse button state
      int2 cursorPositionInt{};
      mouseButtonState = SDL_GetMouseState(&cursorPositionInt.x, &cursorPositionInt.y);
      cursorPosition = float2(cursorPositionInt);
    }

    // Update key mod state
    modifiers = SDL_GetModState();

    pointers.pointers.clear();
    int numDevices = SDL_GetNumTouchDevices();
    for (int d = 0; d < numDevices; d++) {
      auto deviceId = SDL_GetTouchDevice(d);
      int numFingers = SDL_GetNumTouchFingers(deviceId);
      for (int f = 0; f < numFingers; f++) {
        auto *finger = SDL_GetTouchFinger(SDL_GetTouchDevice(d), f);
        if (finger) {
          auto &outFinger = pointers.getOrInsert(finger->id);
          outFinger.position = float2(finger->x, finger->y) * region.size;
          outFinger.pressure = finger->pressure;
          outFinger.touchId = deviceId;
        }
      }
    }
#else
    modifiers = SDL_Keymod(gfx::em::getEventHandler()->serverInputState.modKeyState);
    auto& modifiersBits = reinterpret_cast<uint16_t&>(modifiers);
    auto expandLeftRightBits = [&](SDL_Keymod mod) {
      if ((modifiersBits & mod) != 0)
        modifiersBits |= mod;
    };
    // Expand KMOD_LCTRL, etc. to KMOD_CTRL
    expandLeftRightBits(KMOD_CTRL);
    expandLeftRightBits(KMOD_SHIFT);
    expandLeftRightBits(KMOD_ALT);
    expandLeftRightBits(KMOD_GUI);
#endif
  }
};
} // namespace shards::input

#endif /* AAC121E8_D624_4AD0_81F6_587EB5F716DC */
