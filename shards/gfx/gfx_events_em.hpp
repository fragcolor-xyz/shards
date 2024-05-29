#ifndef A5594113_E6CD_4173_A9A3_3C9492F4FD87
#define A5594113_E6CD_4173_A9A3_3C9492F4FD87

#include <variant>
#include <shards/log/log.hpp>
#include <shards/input/sdl.hpp>
#include <emscripten/dom_pk_codes.h>

// Enable to debug events in logs
#define SHARDS_GFX_EM_LOG_EVENTS 0

namespace gfx::em {
struct KeyEvent {
  // 0 = pressed, 1 = released
  int32_t type_;
  int32_t domKey_;
  // Unicode code point for pressed key, or 0 when it doesn't have one
  uint32_t key_;
  uint8_t location;
  bool ctrlKey;
  bool altKey;
  bool shiftKey;
  bool repeat;
};

struct MouseEvent {
  // 0 = move, 1 = pressed, 2 = released
  int32_t type_;
  int32_t x, y;
  int32_t button;
  int32_t movementX, movementY;
};

struct MouseWheelEvent {
  float deltaY;
};

struct SetCursorMessage {
  int32_t type;
};

using EventVar = std::variant<KeyEvent, MouseEvent, MouseWheelEvent>;
using MessageVar = std::variant<SetCursorMessage>;

SDL_Keycode Emscripten_MapKeyCode(const KeyEvent *keyEvent);

struct EventHandler {
  struct AtomicState {
    int32_t displayWidth{}, displayHeight{};
    int32_t canvasWidth{}, canvasHeight{};
    float pixelRatio = 1.0f;
  } astate;

  struct ServerInputState {
    uint32_t modKeyState{};
  } serverInputState;

  std::mutex mutex;
  std::vector<EventVar> clientEvents;
  std::vector<EventVar> serverEvents;
  std::vector<EventVar> serverEvents1;

  std::string canvasContainerTag;
  std::string canvasTag;

  template <typename T> void serverProcessQueue(T &&cb) {
    {
      std::unique_lock<std::mutex> l(mutex);
      serverEvents1.clear();
      std::swap(serverEvents1, serverEvents);
    }

    for (auto &e : serverEvents1) {
      std::visit(
          [&](auto &arg) {
            using T1 = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T1, KeyEvent>) {
              serverTrackKeyEventInternal(arg);
            }
          },
          e);
      cb(e);
    }
  }

  void serverPostMessage(MessageVar v) {
    // TODO
  }

  void tryFlush() {
    if (mutex.try_lock()) {
      for (auto &e : clientEvents) {
        serverEvents.push_back(e);
      }
      mutex.unlock();
    }
    clientEvents.clear();
  }

  void clientPost(EventVar v) {
    clientEvents.push_back(v);
#if SHARDS_GFX_EM_LOG_EVENTS
    std::visit(
        [](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, MouseEvent>) {
            SPDLOG_INFO("ClientPost MouseEvent type: {}, pos: ({}, {}), "
                        "movement: ({}, {})",
                        arg.type_, arg.x, arg.y, arg.movementX, arg.movementY);
          } else if constexpr (std::is_same_v<T, KeyEvent>) {
            SPDLOG_INFO("ClientPost KeyEvent type: {}, keyCode: {}, ctrl: {}, "
                        "alt: {}, shift: {}, repeat: {}",
                        arg.type_, arg.keyCode, arg.ctrlKey, arg.altKey, arg.shiftKey, arg.repeat);
          } else if constexpr (std::is_same_v<T, MouseWheelEvent>) {
            SPDLOG_INFO("ClientPost MouseWheelEvent dx: {}, dy: {}, dz: {}", arg.deltaX, arg.deltaY, arg.deltaZ);
          }
        },
        v);
#endif
    tryFlush();
  }

private:
  void serverTrackKeyEventInternal(KeyEvent &e) {
    auto applyMod = [&](uint32_t pk, SDL_Keymod km) {
      if (e.domKey_ == pk) {
        if (e.type_ == 0) {
          serverInputState.modKeyState |= km;
        } else {
          serverInputState.modKeyState &= ~km;
        }
      }
    };
    applyMod(DOM_PK_SHIFT_LEFT, SDL_Keymod::KMOD_LSHIFT);
    applyMod(DOM_PK_SHIFT_RIGHT, SDL_Keymod::KMOD_RSHIFT);
    applyMod(DOM_PK_CONTROL_LEFT, SDL_Keymod::KMOD_LCTRL);
    applyMod(DOM_PK_CONTROL_RIGHT, SDL_Keymod::KMOD_RCTRL);
    applyMod(DOM_PK_ALT_LEFT, SDL_Keymod::KMOD_LALT);
    applyMod(DOM_PK_ALT_RIGHT, SDL_Keymod::KMOD_RALT);
  }
};

EventHandler *getEventHandler();
void eventHandlerPostKeyEvent(EventHandler *eh, const KeyEvent &ke);
void eventHandlerPostMouseEvent(EventHandler *eh, const MouseEvent &pe);
void eventHandlerPostWheelEvent(EventHandler *eh, const MouseWheelEvent &we);
void eventHandlerPostDisplayFormat(EventHandler *eh, int32_t width, int32_t height, int32_t cwidth, int32_t cheight,
                                   float pixelRatio);
} // namespace gfx::em

#endif /* A5594113_E6CD_4173_A9A3_3C9492F4FD87 */
