#ifndef F300A3B7_A146_45E8_9B54_65F54816979E
#define F300A3B7_A146_45E8_9B54_65F54816979E

#include "events.hpp"
#include "state.hpp"

#if !SHARDS_GFX_SDL
#include <shards/gfx/gfx_events_em.hpp>
#include <utf8.h/utf8.h>
#endif

namespace shards::input {
struct NativeEventDecoderBuffer {

  float scrollDelta{};
  boost::container::string text;
  void reset() {
    scrollDelta = 0.0f;
    text.clear();
  }
};

struct NativeEventDecoder {
  std::vector<Event> &virtualInputEvents;
  const InputState &state;
  InputState &newState;
  NativeEventDecoderBuffer& buffer;

  // Apply Native events to the new state
#if SHARDS_GFX_SDL
  using NativeEventType = SDL_Event;
#else
  using NativeEventType = gfx::em::EventVar;
#endif

  void apply(const NativeEventType &event);
};
} // namespace shards::input

#endif /* F300A3B7_A146_45E8_9B54_65F54816979E */
