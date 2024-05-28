#ifndef BB4427B0_5641_4FB1_86E3_6E9D1AD7C986
#define BB4427B0_5641_4FB1_86E3_6E9D1AD7C986
#include "input.hpp"
#include "input/events.hpp"
#include "state.hpp"
#include "events.hpp"
#include "log.hpp"
#include "sdl.hpp"
#include <boost/container/string.hpp>
#include <boost/container/flat_map.hpp>
#include <type_traits>

#if !SHARDS_GFX_SDL
#include <shards/gfx/gfx_events_em.hpp>
#endif

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

  // Given a callback function that receives an apply function, update the state and synthesize events
  // the apply function receives a consumable event that is applied to the input state
  //
  // Example:
  //  input.update([&](auto& apply) {
  //   for(auto& event : events) {
  //     apply(event, true /* hasFocus */);
  //   }
  //  });
  using UpdateApplyFn = decltype([](const ConsumableEvent &consumableEvent, bool hasFocus) {});
  template <typename T> std::enable_if_t<std::is_invocable_v<T, UpdateApplyFn>> update(T callback) {
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

// Apply Native events to the new state
#if SHARDS_GFX_SDL
  using NativeEventType = SDL_Event;
#else
  using NativeEventType = gfx::em::EventVar;
#endif

  using UpdateApplyFn2 = decltype([](const NativeEventType &evt) {});
  template <typename T> std::enable_if_t<std::is_invocable_v<T, UpdateApplyFn2>> updateNative(T callback) {
    swapBuffers();

    virtualInputEvents.clear();

    InputState newState = state;
    newState.update();
    auto applyFn = [&](const NativeEventType &event) {
      apply(event, newState); //
    };
    callback(applyFn);

    endUpdate(newState);
  }

  void update(InputState &state) {
    beginUpdate();
    endUpdate(state);
  }

private:
  // called before endUpdate, apply() can be called with SDL_Events after this
  void beginUpdate() { virtualInputEvents.clear(); }

  // Updates the input state and synthesizes events based on
  //  the difference between the previous state and the new state
  void endUpdate(const InputState &inputState) {
    swapBuffers();

    synthesizeKeyUpEvents(inputState);

    synthesizeTouchUpEvents(inputState);

    if constexpr (Pointer::HasPersistentPointer) {
      synthesizeMouseButtonEvents(inputState);
      synthesizeMouseEvents(inputState);
    } else {
      synthesizeMouseFromTouchEvents(inputState);
    }

    state = inputState;

    if constexpr (!Pointer::HasPersistentPointer) {
      updateCursorPositionFromPointerState(state);
    }
  }

#if SHARDS_GFX_SDL
  // Apply an event to the new state, during update
  void apply(const SDL_Event &event, InputState &newState) {
    auto &buffer = buffers[getBufferIndex(1)];
    if (event.type == SDL_MOUSEWHEEL) {
      buffer.scrollDelta += event.wheel.preciseY;
    } else if (event.type == SDL_TEXTEDITING) {
      auto &ievent = event.edit;

      if (strlen(ievent.text) > 0) {
        if (!imeComposing) {
          imeComposing = true;
        }

        virtualInputEvents.push_back(TextCompositionEvent{.text = ievent.text});
      }
    } else if (event.type == SDL_KEYDOWN) {
      virtualInputEvents.push_back(
          KeyEvent{.key = event.key.keysym.sym, .pressed = true, .modifiers = state.modifiers, .repeat = event.key.repeat});
    } else if (event.type == SDL_TEXTINPUT) {
      auto &ievent = event.text;
      if (imeComposing) {
        virtualInputEvents.push_back(TextCompositionEndEvent{.text = ievent.text});
        imeComposing = false;
      } else {
        virtualInputEvents.push_back(TextEvent{.text = ievent.text});
      }
    } else if (event.type == SDL_WINDOWEVENT) {
      if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
        virtualInputEvents.push_back(RequestCloseEvent{});
      } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
      }
    } else if (event.type == SDL_APP_DIDENTERBACKGROUND) {
      virtualInputEvents.push_back(SupendEvent{});
    } else if (event.type == SDL_APP_DIDENTERFOREGROUND) {
      virtualInputEvents.push_back(ResumeEvent{});
    } else if (event.type == SDL_FINGERMOTION) {
      auto &ievent = event.tfinger;
      virtualInputEvents.push_back(PointerTouchMoveEvent{
          .pos = float2(ievent.x, ievent.y) * state.region.size,
          .delta = float2(event.tfinger.dx, event.tfinger.dy) * state.region.size,
          .index = ievent.fingerId,
          .pressure = ievent.pressure,
      });
    } else if (event.type == SDL_FINGERDOWN) {
      auto &ievent = event.tfinger;

      auto &pointer = newState.pointers.getOrInsert(ievent.fingerId);
      pointer.position = float2(ievent.x, ievent.y) * state.region.size;
      pointer.pressure = ievent.pressure;
      pointer.touchId = ievent.touchId;

      virtualInputEvents.push_back(PointerTouchEvent{
          .pos = float2(ievent.x, ievent.y) * state.region.size,
          .delta = float2(event.tfinger.dx, event.tfinger.dy) * state.region.size,
          .index = ievent.fingerId,
          .pressure = ievent.pressure,
          .pressed = true,
      });
    } else if (event.type == SDL_DROPFILE) {
      virtualInputEvents.push_back(DropFileEvent{.path = event.drop.file});
      SPDLOG_LOGGER_DEBUG(getLogger(), "Window dropped file: {}", event.drop.file);
      if (event.drop.file)
        SDL_free(event.drop.file);
    }
  }
#else
  void apply(const gfx::em::EventVar &event, InputState &newState) {
    auto &buffer = buffers[getBufferIndex(1)];
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, gfx::em::KeyEvent>) {
            virtualInputEvents.push_back(KeyEvent{
                .key = arg.key_, .pressed = arg.type_ == 0, .modifiers = state.modifiers, .repeat = arg.repeat ? 1u : 0u});
          } else if constexpr (std::is_same_v<T, gfx::em::MouseEvent>) {
            uint8_t buttonIndex = (arg.button + 1);
            if (arg.type_ == 0) {
              virtualInputEvents.push_back(PointerMoveEvent{.pos = float2(arg.x, arg.y)});
            } else {
              virtualInputEvents.push_back(
                  PointerButtonEvent{.pos = float2(arg.x, arg.y), .index = buttonIndex, .pressed = (arg.type_ == 1)});
            }
          } else if constexpr (std::is_same_v<T, gfx::em::MouseWheelEvent>) {
            buffer.scrollDelta += arg.deltaY;
          }
        },
        event);
  }
#endif

  // Apply a consumable event to this detached input state
  // any non-consumed event will be handled normally
  // consumed events will only contribute to loss-of-focus & button/key release events
  void apply(const ConsumableEvent &consumableEvent, bool hasFocus, InputState &newState) {
    bool consumed = consumableEvent.isConsumed();
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
          } else if constexpr (std::is_same_v<T, PointerTouchMoveEvent>) {
            if (!consumed) {
              if (auto ptr = newState.pointers.get(arg.index)) {
                ptr->get().position = arg.pos;
              }
            }
          } else if constexpr (std::is_same_v<T, PointerTouchEvent>) {
            if (!consumed && hasFocus && arg.pressed && !newState.pointers.get(arg.index)) {
              auto &pointer = newState.pointers.getOrInsert(arg.index);
              pointer.position = arg.pos;
              pointer.pressure = arg.pressure;
              virtualInputEvents.push_back(PointerTouchEvent{
                  .pos = pointer.position,
                  .index = arg.index,
                  .pressed = true,
              });
            }
            if (!arg.pressed && newState.pointers.get(arg.index)) {
              newState.pointers.erase(arg.index);
              virtualInputEvents.push_back(PointerTouchEvent{
                  .pos = arg.pos,
                  .index = arg.index,
                  .pressed = false,
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
            if (newState.isKeyHeld(arg.key)) {
              if (!arg.pressed) {
                newState.heldKeys.erase(arg.key);
                virtualInputEvents.push_back(KeyEvent{
                    .key = arg.key,
                    .pressed = false,
                    .modifiers = arg.modifiers,
                });
              } else if (arg.repeat > 0) {
                virtualInputEvents.push_back(KeyEvent{
                    .key = arg.key,
                    .pressed = true,
                    .modifiers = arg.modifiers,
                    .repeat = arg.repeat,
                });
              }
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

  void synthesizeTouchUpEvents(const InputState &inputState) {
    for (auto &[k, v] : state.pointers.pointers) {
      if (!inputState.pointers.get(k)) {
        virtualInputEvents.push_back(PointerTouchEvent{
            .pos = v.position,
            .index = k,
            .pressed = false,
        });
      }
    }
  }

  void synthesizeMouseFromTouchEvents(const InputState &inputState) {
    auto &oldState = state.pointers;
    auto &newState = inputState.pointers;
    if (oldState.any() != newState.any()) {
      bool pressed = newState.any();
      virtualInputEvents.push_back(PointerButtonEvent{
          .pos = pressed ? newState.centroid() : oldState.centroid(),
          .index = SDL_BUTTON_LEFT,
          .pressed = pressed,
          .modifiers = inputState.modifiers,
      });
    }

    if (oldState.any() && newState.any()) {
      float2 oldPos = oldState.centroid();
      float2 newPos = newState.centroid();
      if (oldPos != newPos) {
        float2 delta = newPos - oldPos;
        virtualInputEvents.push_back(PointerMoveEvent{.pos = newPos, .delta = delta});
      }
    }
  }

  void updateCursorPositionFromPointerState(InputState &inputState) {
    if (inputState.pointers.any())
      inputState.cursorPosition = inputState.pointers.centroid();
  }
};
} // namespace shards::input

#endif /* BB4427B0_5641_4FB1_86E3_6E9D1AD7C986 */
