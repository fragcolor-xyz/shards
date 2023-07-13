#ifndef B01B265C_2D67_4D6C_8DD1_D697ED304EC6
#define B01B265C_2D67_4D6C_8DD1_D697ED304EC6

#include "events.hpp"
#include "event_buffer.hpp"
#include "detached.hpp"
#include "input/events.hpp"
#include "messages.hpp"
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>

namespace gfx {
struct Window;
}

namespace shards::input {
struct FocusTracker {
  void *previous{};
  void *current{};

  void swap() {
    previous = current;
    current = nullptr;
  }

  // Checks current or previous acquisition of focus
  bool hasFocus(void *token) const { return token == current || (!current && token == previous); }

  // Check is the target can receive input based on focus rules
  // If no element has requested any focus, any element can receive input
  bool canReceiveInput(void *token) const { return token == current || (!current && (!previous || token == previous)); }

  // Request exclusive focus on the given element
  bool requestFocus(void *token) {
    // Same frame early out
    if (current == token)
      return true;

    // Obtain new focus or continue focus from last frame
    if (!current && (!previous || previous == token)) {
      current = token;
      return true;
    } else {
      return false;
    }
  }
};

struct IInputHandler {
  virtual ~IInputHandler() = default;
  virtual int getPriority() const { return 0; }
  virtual void handle(const InputState &state, std::vector<ConsumableEvent> &events, FocusTracker &focusTracker) = 0;
};

struct InputMaster {
private:
  std::vector<std::weak_ptr<IInputHandler>> handlers;
  std::vector<std::shared_ptr<IInputHandler>> handlersLocked;
  std::shared_mutex mutex;

  EventBuffer<Frame<ConsumableEvent>> eventBuffer;

  boost::lockfree::spsc_queue<Message> messageQueue;

  FocusTracker focusTracker;

  DetachedInput input;
  InputState state;

  bool terminateRequested{};

public:
  InputMaster();
  ~InputMaster();

  bool isTerminateRequested() const { return terminateRequested; }

  void postMessage(const Message &message) { messageQueue.push(message); }

  void addHandler(std::shared_ptr<IInputHandler> ptr) {
    std::unique_lock<decltype(mutex)> l(mutex);
    handlers.emplace_back(ptr);
  }

  void update(gfx::Window &window);
  void reset() {}

  // Only use from main thread
  void getHandlers(std::vector<std::shared_ptr<IInputHandler>> &outVec) {
    std::unique_lock<decltype(mutex)> l(mutex);
    updateAndSortHandlersLocked(outVec);
  }

  const InputState &getState() const { return state; }
  const std::vector<Event> &getEvents() const { return input.virtualInputEvents; }

private:
  void updateAndSortHandlers();

  // Assumes already locked
  void updateAndSortHandlersLocked(std::vector<std::shared_ptr<IInputHandler>> &outVec);
};

} // namespace shards::input

#endif /* B01B265C_2D67_4D6C_8DD1_D697ED304EC6 */