#ifndef D10804BA_9B46_4108_A3F4_80EC952E3E2B
#define D10804BA_9B46_4108_A3F4_80EC952E3E2B

#include <variant>
#include <compare>
#include <boost/container/string.hpp>
#include "../gfx/linalg.hpp"
#include "sdl.hpp"

namespace shards::input {
// Request application to begin accepting text input
struct BeginTextInputMessage {
  SDL_Rect inputRect{};

  std::partial_ordering operator<=>(const BeginTextInputMessage &other) const {
    if (inputRect.x != other.inputRect.x)
      return inputRect.x <=> other.inputRect.x;
    if (inputRect.y != other.inputRect.y)
      return inputRect.y <=> other.inputRect.y;
    if (inputRect.w != other.inputRect.w)
      return inputRect.w <=> other.inputRect.w;
    if (inputRect.h != other.inputRect.h)
      return inputRect.h <=> other.inputRect.h;
    return std::partial_ordering::equivalent;
  }
};

// Request application to end accepting text input
struct EndTextInputMessage {
  std::partial_ordering operator<=>(const EndTextInputMessage &other) const = default;
};

// Request application to change the cursor
struct SetCursorMessage {
  bool visible = true;
  SDL_SystemCursor cursor = SDL_SystemCursor::SDL_SYSTEM_CURSOR_DEFAULT;
  std::partial_ordering operator<=>(const SetCursorMessage &other) const = default;
};

// Tell the main window loop to terminate
struct TerminateMessage {};

// Tell the main window loop to terminate
struct ResizeWindowMessage {
  gfx::int2 newSize;
};

using Message = std::variant<BeginTextInputMessage, EndTextInputMessage, SetCursorMessage, TerminateMessage, ResizeWindowMessage>;

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

#if !SH_ANDROID
// Some platforms need these, on android 33 they don't compile
inline bool operator==(const Message &a, const Message &b) { return a <=> b == std::partial_ordering::equivalent; }
#endif

} // namespace shards::input
#endif /* D10804BA_9B46_4108_A3F4_80EC952E3E2B */
