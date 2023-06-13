#include "master.hpp"
#include "cursor_map.hpp"
#include "debug.hpp"
#include "window_input.hpp"
#include <spdlog/spdlog.h>
#include <gfx/window.hpp>
#include <SDL_keyboard.h>
#include <SDL_events.h>

namespace shards::input {

// Add a singleton check since SDL_PollEvent is global
// having more than one of these would break input distribution
static std::atomic<InputMaster *> instance{};

InputMaster::InputMaster() : messageQueue(128) {
  if (instance != nullptr)
    throw std::logic_error("Only one InputMaster can exist at a time.");
  instance = this;
}

InputMaster::~InputMaster() {
  assert(instance == this &&
         "Expected InputMaster instance to be the only instance, but it's singleton was modified before destruction.");
  instance = nullptr;
}

void InputMaster::update(gfx::Window &window) {
  updateAndSortHandlers();

  input.beginUpdate();
  bool hasEvent = false;
  do {
    SDL_Event event{};
    hasEvent = SDL_PollEvent(&event) > 0;

    input.apply(event);
  } while (hasEvent);

  state.update();
  state.region = getWindowInputRegion(window);

  input.endUpdate(state);

  // Convert events to ConsumableEvents
  auto &evtFrame = eventBuffer.getNextFrame();
  evtFrame.state = state;

  for (auto &evt : input.virtualInputEvents) {
    if (!std::get_if<PointerMoveEvent>(&evt))
      SPDLOG_INFO("Generated event: {}", debugFormat(evt));
    evtFrame.events.emplace_back(evt);
  }

  // Call handlers in order of priority
  focusTracker.swap();
  for (auto &handler : handlersLocked) {
    handler->handle(state, evtFrame.events, focusTracker);
  }

  // Mark new frame as available in the event buffer
  eventBuffer.nextFrame();

  // Handle posted messages
  messageQueue.consume_all([&](const Message &message) {
    SPDLOG_INFO("Handling message: {}", debugFormat(message));
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, BeginTextInputMessage>) {
            SDL_StartTextInput();
          } else if constexpr (std::is_same_v<T, EndTextInputMessage>) {
            SDL_StopTextInput();
          } else if constexpr (std::is_same_v<T, SetCursorMessage>) {
            if (!arg.visible) {
              SDL_ShowCursor(SDL_DISABLE);
            } else {
              auto &cursorMap = CursorMap::getInstance();
              SDL_ShowCursor(SDL_ENABLE);
              SDL_SetCursor(cursorMap.getCursor(arg.cursor));
            }
          } else if constexpr (std::is_same_v<T, TerminateMessage>) {
            terminateRequested = true;
          }
        },
        message);
  });
}

void InputMaster::updateAndSortHandlers() {
  std::shared_lock<decltype(mutex)> l(mutex);
  updateAndSortHandlersLocked(handlersLocked);
}

void InputMaster::updateAndSortHandlersLocked(std::vector<std::shared_ptr<IInputHandler>> &outVec) {
  outVec.clear();
  for (auto it = handlers.begin(); it != handlers.end();) {
    auto ptr = it->lock();
    if (ptr) {
      outVec.insert(std::upper_bound(outVec.begin(), outVec.end(), ptr,
                                     [](const auto &a, const auto &b) { return a->getPriority() > b->getPriority(); }),
                    ptr);
      ++it;
    } else {
      it = handlers.erase(it);
    }
  }
}

} // namespace shards::input