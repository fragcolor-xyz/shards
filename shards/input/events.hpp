#ifndef A58BF89A_157D_440C_9398_773EB3D0A410
#define A58BF89A_157D_440C_9398_773EB3D0A410
#include "input.hpp"
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <compare>
#include <linalg.h>
#include <optional>
#include <variant>
#include "../core/platform.hpp"
#include <boost/container/string.hpp>

// Defines the primary command key
//   on apple this is the cmd key
//   otherwise the ctrl key
#if SH_APPLE
#define KMOD_PRIMARY KMOD_GUI
#define KMOD_SECONDARY KMOD_CTRL
#else
#define KMOD_PRIMARY KMOD_CTRL
#define KMOD_SECONDARY KMOD_GUI
#endif

namespace shards::input {
using namespace linalg::aliases;

struct PointerMoveEvent {
  float2 pos;
  float2 delta;

  std::partial_ordering operator<=>(const PointerMoveEvent &other) const = default;
};

struct PointerButtonEvent {
  float2 pos;
  int index;
  bool pressed;
  SDL_Keymod modifiers;

  std::partial_ordering operator<=>(const PointerButtonEvent &other) const = default;
};

struct ScrollEvent {
  float delta;

  std::partial_ordering operator<=>(const ScrollEvent &other) const = default;
};

struct KeyEvent {
  SDL_Keycode key;
  bool pressed;
  SDL_Keymod modifiers;
  uint32_t repeat;

  std::partial_ordering operator<=>(const KeyEvent &other) const = default;
};

struct SupendEvent {
  std::partial_ordering operator<=>(const KeyEvent &other) const { return std::partial_ordering::equivalent; }
};

struct ResumeEvent {
  std::partial_ordering operator<=>(const KeyEvent &other) const { return std::partial_ordering::equivalent; }
};

struct InputRegionEvent {
  InputRegion region;

  std::partial_ordering operator<=>(const KeyEvent &other) const { return std::partial_ordering::equivalent; }
};

struct TextEvent {
  boost::container::string text;

  std::partial_ordering operator<=>(const TextEvent &other) const = default;
};

struct TextCompositionEvent {
  boost::container::string text;

  std::partial_ordering operator<=>(const TextCompositionEvent &other) const = default;
};

struct TextCompositionEndEvent {
  boost::container::string text;

  std::partial_ordering operator<=>(const TextCompositionEndEvent &other) const = default;
};

// Posted when the user or OS attempts to close the window
struct RequestCloseEvent {
  std::partial_ordering operator<=>(const RequestCloseEvent &other) const = default;
};

using Event = std::variant<PointerMoveEvent, PointerButtonEvent, ScrollEvent, KeyEvent, SupendEvent, ResumeEvent,
                           InputRegionEvent, TextEvent, TextCompositionEvent, TextCompositionEndEvent, RequestCloseEvent>;

inline std::partial_ordering operator<=>(const Event &a, const Event &b) {
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
inline bool operator==(const Event &a, const Event &b) { return a <=> b == std::partial_ordering::equivalent; }

struct IInputHandler;
struct ConsumedTag {
  std::weak_ptr<IInputHandler> handler;
  ConsumedTag(std::weak_ptr<IInputHandler> handler) : handler(handler) {}
};

struct ConsumableEvent {
  Event event;
  std::optional<ConsumedTag> consumed;

  ConsumableEvent(const Event &event) : event(event) {}
  bool isConsumed() const { return consumed.has_value(); }
  void consume(std::weak_ptr<IInputHandler> by) { consumed.emplace(by); }
};

} // namespace shards::input

#endif /* A58BF89A_157D_440C_9398_773EB3D0A410 */
