#ifndef BB4427B0_5641_4FB1_86E3_6E9D1AD7C986
#define BB4427B0_5641_4FB1_86E3_6E9D1AD7C986
#include "input.hpp"
#include "state.hpp"
#include "events.hpp"
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <boost/container/string.hpp>

namespace shards::input {
// Keeps track of all input state separately and synthesizes it's own events by
// diffing the previous state with the new state
struct DetachedInput {
  // Events
  std::vector<Event> virtualInputEvents;

  InputState state;

  struct Buffer {
    float scrollDelta{};
    boost::container::string text;
    void reset() {
      scrollDelta = 0.0f;
      text.clear();
    }
  } buffers[2];

  size_t currentBufferIndex{};
  bool imeComposing{};

public:
  float getScrollDelta() const { return buffers[currentBufferIndex].scrollDelta; }

  // Apply an event to the new state (after update())
  void apply(const SDL_Event &event) {
    auto &buffer = buffers[getBufferIndex(1)];
    if (event.type == SDL_MOUSEWHEEL) {
      buffer.scrollDelta = event.wheel.preciseY;
    } else if (event.type == SDL_TEXTEDITING) {
      auto &ievent = event.edit;

      if (strlen(ievent.text) > 0) {
        if (!imeComposing) {
          imeComposing = true;
        }

        virtualInputEvents.push_back(TextCompositionEvent{.text = ievent.text});
      }
    } else if (event.type == SDL_TEXTINPUT) {
      auto &ievent = event.text;
      if (imeComposing) {
        virtualInputEvents.push_back(TextCompositionEndEvent{.text = ievent.text});
        imeComposing = false;
      } else {
        virtualInputEvents.push_back(TextEvent{.text = ievent.text});
      }
    }
  }

  // Given a callback function that receives an apply function, update the state and synthesize events
  // the apply function receives a consumable event that is applied to the input state
  //
  // Example:
  //  input.update([&](auto& apply) {
  //   for(auto& event : events) {
  //     apply(event, true /* hasFocus */);
  //   }
  //  });
  template <typename T> void update(T callback) {
    swapBuffers();

    virtualInputEvents.clear();

    InputState newState = state;
    auto applyFn = [&](const ConsumableEvent &consumableEvent, bool hasFocus) {
      apply(consumableEvent, hasFocus, newState); //
    };
    callback(applyFn);

    // NOTE: These events are already translated from the original
    // synthesizeKeyUpEvents(newState);
    // synthesizeKeyDownEvents(newState);
    // synthesizeMouseButtonEvents(newState);
    synthesizeMouseEvents(newState);

    state = newState;
  }

  // called before endUpdate, apply() can be called with SDL_Events after this
  void beginUpdate() { virtualInputEvents.clear(); }

  // Updates the input state and synthesizes events based on
  //  the difference between the previous state and the new state
  void endUpdate(const InputState &inputState) {
    swapBuffers();

    synthesizeKeyUpEvents(inputState);
    synthesizeKeyDownEvents(inputState);
    synthesizeMouseButtonEvents(inputState);
    synthesizeMouseEvents(inputState);

    state = inputState;
  }

private:
  // Apply a consumable event to this detached input state
  // any non-consumed event will be handled normally
  // consumed events will only contribute to loss-of-focus & button/key release events
  void apply(const ConsumableEvent &consumableEvent, bool hasFocus, InputState &newState) {
    bool consumed = consumableEvent.consumed;
    auto &buffer = buffers[getBufferIndex(1)];
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, PointerMoveEvent>) {
            if (!consumed)
              newState.cursorPosition = arg.pos;
          } else if constexpr (std::is_same_v<T, PointerButtonEvent>) {
            if (!consumed && hasFocus && arg.pressed && !newState.isMouseButtonHeld(arg.index)) {
              newState.mouseButtonState |= SDL_BUTTON(arg.index);
              virtualInputEvents.push_back(PointerButtonEvent{
                  .pos = newState.cursorPosition,
                  .index = arg.index,
                  .pressed = true,
                  .modifiers = arg.modifiers,
              });
            }
            if (!arg.pressed && newState.isMouseButtonHeld(arg.index)) {
              newState.mouseButtonState &= ~uint32_t(SDL_BUTTON(arg.index));
              virtualInputEvents.push_back(PointerButtonEvent{
                  .pos = newState.cursorPosition,
                  .index = arg.index,
                  .pressed = false,
                  .modifiers = arg.modifiers,
              });
            }
          } else if constexpr (std::is_same_v<T, KeyEvent>) {
            if (!consumed && hasFocus && arg.pressed && !newState.isKeyHeld(arg.key)) {
              newState.heldKeys.insert(arg.key);
              virtualInputEvents.push_back(KeyEvent{
                  .key = arg.key,
                  .pressed = true,
                  .modifiers = arg.modifiers,
              });
            }
            if (!arg.pressed && newState.isKeyHeld(arg.key)) {
              newState.heldKeys.erase(arg.key);
              virtualInputEvents.push_back(KeyEvent{
                  .key = arg.key,
                  .pressed = false,
                  .modifiers = arg.modifiers,
              });
            }
          } else if constexpr (std::is_same_v<T, ScrollEvent>) {
            if (!consumed && hasFocus)
              buffer.scrollDelta += arg.delta;
          } else if constexpr (std::is_same_v<T, SupendEvent>) {
            newState.suspended = true;
            // TODO
          } else if constexpr (std::is_same_v<T, ResumeEvent>) {
            newState.suspended = false;
            // TODO
          } else if constexpr (std::is_same_v<T, InputRegionEvent>) {
            newState.region = arg.region;
          } else if constexpr (std::is_same_v<T, TextEvent>) {
            virtualInputEvents.push_back(TextEvent{.text = arg.text});
          } else if constexpr (std::is_same_v<T, TextCompositionEvent>) {
            virtualInputEvents.push_back(TextCompositionEvent{.text = arg.text});
          } else if constexpr (std::is_same_v<T, TextCompositionEndEvent>) {
            virtualInputEvents.push_back(TextCompositionEndEvent{.text = arg.text});
          }
        },
        consumableEvent.event);
  }

  size_t getBufferIndex(int offset) const { return size_t((int(currentBufferIndex) + offset) % std::size(buffers)); }
  void swapBuffers() {
    buffers[currentBufferIndex].reset();
    currentBufferIndex = getBufferIndex(1);
  }

  void synthesizeKeyUpEvents(const InputState &inputState) {
    for (auto key : state.heldKeys) {
      if (!inputState.isKeyHeld(key)) {
        virtualInputEvents.push_back(KeyEvent{
            .key = key,
            .pressed = false,
            .modifiers = inputState.modifiers,
        });
      }
    }
  }

  void synthesizeKeyDownEvents(const InputState &inputState) {
    for (auto key : inputState.heldKeys) {
      if (!state.heldKeys.contains(key)) {
        virtualInputEvents.push_back(KeyEvent{
            .key = key,
            .pressed = true,
            .modifiers = inputState.modifiers,
        });
      }
    }
  }

  void synthesizeMouseButtonEvents(const InputState &inputState) {
    uint32_t oldState = state.mouseButtonState;
    uint32_t newState = inputState.mouseButtonState;
    if (oldState != newState) {
      for (int i = 1; i <= NumMouseButtons; i++) {
        if ((newState & SDL_BUTTON(i)) != 0 && (oldState & SDL_BUTTON(i)) == 0) {
          virtualInputEvents.push_back(PointerButtonEvent{
              .pos = inputState.cursorPosition,
              .index = i,
              .pressed = true,
              .modifiers = inputState.modifiers,
          });
        } else if ((newState & SDL_BUTTON(i)) == 0 && (oldState & SDL_BUTTON(i)) != 0) {
          virtualInputEvents.push_back(PointerButtonEvent{
              .pos = inputState.cursorPosition,
              .index = i,
              .pressed = false,
              .modifiers = inputState.modifiers,
          });
        }
      }
    }
  }

  void synthesizeMouseEvents(const InputState &inputState) {
    if (inputState.cursorPosition != state.cursorPosition) {
      float2 delta = inputState.cursorPosition - state.cursorPosition;
      virtualInputEvents.push_back(PointerMoveEvent{.pos = inputState.cursorPosition, .delta = delta});
    }

    if (getScrollDelta() != 0.0f) {
      virtualInputEvents.push_back(ScrollEvent{.delta = getScrollDelta()});
    }
  }
};
} // namespace shards::input

#endif /* BB4427B0_5641_4FB1_86E3_6E9D1AD7C986 */
