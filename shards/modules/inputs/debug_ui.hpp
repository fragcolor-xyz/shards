#ifndef D9559C5B_F728_4B73_B3D2_372D625A794D
#define D9559C5B_F728_4B73_B3D2_372D625A794D

#include <stdint.h>
#include <stddef.h>

namespace shards::input::debug {
using OpaqueEvent = void *;

struct ConsumeFlags {
  bool canReceiveInput;
  bool requestFocus;
  bool wantsPointerInput;
  bool wantsKeyboardInput;
};

struct Layer {
  bool focused;
  int priority;
  const char *name;

  OpaqueEvent *debugEvents;
  size_t numDebugEvents;

  ConsumeFlags consumeFlags;
};

} // namespace shards::input::debug

#ifndef RUST_BINDGEN
#include <shards/input/event_buffer.hpp>
namespace shards::input::debug {

struct IDebug {
  virtual const char *getDebugName() = 0;
  virtual const std::vector<input::ConsumableEvent> *getDebugFrame(size_t frameIndex) = 0;
  virtual size_t getLastFrameIndex() const = 0;
  virtual ConsumeFlags getDebugConsumeFlags() const = 0;
};
} // namespace shards::input::debug
extern "C" {
struct SHVar;
void shards_input_showDebugUI(SHVar *context, shards::input::debug::Layer *layers, size_t layerCount);
}
#else
extern "C" {
struct SHVar;
const char *shards_input_eventToString(shards::input::debug::OpaqueEvent);
void shards_input_freeString(const char *str);
bool shards_input_eventIsConsumed(shards::input::debug::OpaqueEvent);
}
#endif
#endif /* D9559C5B_F728_4B73_B3D2_372D625A794D */
