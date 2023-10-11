#include "master.hpp"
#include "cursor_map.hpp"
#include "debug.hpp"
#include "window_input.hpp"
#include "log.hpp"
#include <thread>
#include <spdlog/spdlog.h>
#include <gfx/window.hpp>
#include <SDL_keyboard.h>
#include <SDL_events.h>

namespace shards::input {

static auto logger = getLogger();

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
  thisThreadId = std::this_thread::get_id();

  input.beginUpdate();
  do {
    SDL_Event event{};
    bool hasEvent = SDL_PollEvent(&event) > 0;
    if (hasEvent)
      input.apply(event);
    else
      break;
  } while (true);
  state.update();
  state.region = getWindowInputRegion(window);
  input.endUpdate(state);

  // Convert events to ConsumableEvents
  events.clear();
  for (auto &evt : input.virtualInputEvents) {
    if (!std::get_if<PointerMoveEvent>(&evt))
      SPDLOG_LOGGER_DEBUG(logger, "Generated event: {}", debugFormat(evt));
    events.emplace_back(evt);
  }

  // Call handlers in order of priority
  focusTracker.swap();
  updateAndSortHandlers();
  for (auto &handler : handlersLocked) {
    handler->handle(*this);
  }
  handlersLocked.clear();

  // Handle posted messages
  messageQueue.consume_all([&](const Message &message) {
    handleMessage(message);
  });

  for(auto& cb : postInputCallbacks) {
    cb(*this);
  }
  postInputCallbacks.clear();
}

void InputMaster::postMessage(const Message &message) {
  if (std::this_thread::get_id() == thisThreadId)
    handleMessage(message);
  else
    messageQueue.push(message);
}

void InputMaster::handleMessage(const Message &message) {
  SPDLOG_LOGGER_DEBUG(logger, "Handling message: {}", debugFormat(message));
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