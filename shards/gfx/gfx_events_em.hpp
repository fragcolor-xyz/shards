#ifndef A5594113_E6CD_4173_A9A3_3C9492F4FD87
#define A5594113_E6CD_4173_A9A3_3C9492F4FD87

#include <variant>
#include <shards/log/log.hpp>

// Enable to debug events in logs
#define SHARDS_GFX_EM_LOG_EVENTS 0

namespace gfx::em {
struct KeyEvent {
  int32_t type_;
  int32_t key_;
  int32_t scan_;
  bool ctrlKey;
  bool altKey;
  bool shiftKey;
  bool repeat;
};

struct MouseEvent {
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

struct EventHandler {
  struct AtomicState {
    int32_t displayWidth{}, displayHeight{};
    int32_t canvasWidth{}, canvasHeight{};
    float pixelRatio = 1.0f;
  } astate;

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
};

EventHandler *getEventHandler();
void eventHandlerPostKeyEvent(EventHandler *eh, const KeyEvent &ke);
void eventHandlerPostMouseEvent(EventHandler *eh, const MouseEvent &pe);
void eventHandlerPostWheelEvent(EventHandler *eh, const MouseWheelEvent &we);
void eventHandlerPostDisplayFormat(EventHandler *eh, int32_t width, int32_t height, int32_t cwidth, int32_t cheight,
                                   float pixelRatio);
} // namespace gfx::em

#endif /* A5594113_E6CD_4173_A9A3_3C9492F4FD87 */
