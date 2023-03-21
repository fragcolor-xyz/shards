#ifndef D10804BA_9B46_4108_A3F4_80EC952E3E2B
#define D10804BA_9B46_4108_A3F4_80EC952E3E2B

#include <variant>
#include <boost/container/string.hpp>

namespace shards::input {
// Request application to begin accepting text input
struct BeginTextInputMessage {
  SDL_Rect inputRect{};
  std::partial_ordering operator<=>(const BeginTextInputMessage &other) const {
    return std::tie(inputRect.x, inputRect.y, inputRect.w, inputRect.h) <=>
           std::tie(other.inputRect.x, other.inputRect.y, other.inputRect.w, other.inputRect.h);
  }
};

// Request application to end accepting text input
struct EndTextInputMessage {
  std::partial_ordering operator<=>(const EndTextInputMessage &other) const = default;
};

// Request application to change the cursor
struct SetCursorMessage {
  bool visible = true;
  SDL_SystemCursor cursor = SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW;
  std::partial_ordering operator<=>(const SetCursorMessage &other) const = default;
};

// Tell the main window loop to terminate
struct TerminateMessage {};

using Message = std::variant<BeginTextInputMessage, EndTextInputMessage, SetCursorMessage, TerminateMessage>;

inline std::partial_ordering operator<=>(const Message &a, const Message &b) {
  auto ci = a.index() <=> b.index();
  if (ci != 0)
    return ci;

  return std::visit(
      [&](auto &arg) {
        using T = std::decay_t<decltype(arg)>;
        return arg <=> std::get<T>(b);
      },
      a);
}
inline bool operator==(const Message &a, const Message &b) { return a <=> b == std::partial_ordering::equivalent; }

} // namespace shards::input
#endif /* D10804BA_9B46_4108_A3F4_80EC952E3E2B */
