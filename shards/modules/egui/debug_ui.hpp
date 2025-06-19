#ifndef D9559C5B_F728_4B73_B3D2_372D625A794D
#define D9559C5B_F728_4B73_B3D2_372D625A794D

#include <stdint.h>
#include <stddef.h>

namespace shards::input::debug {
using OpaqueEvent = void *;
using OpaqueLayer = void*;

struct ConsumeFlags {
  bool canReceiveInput;
  bool requestFocus;
  bool wantsPointerInput;
  bool wantsKeyboardInput;
};

struct Layer {
  int priority;
  const char *name;
  bool hasFocus{};
};

struct TaggedEvent {
  size_t frameIndex;
  OpaqueEvent evt;
};

// Persistent
struct DebugUIOpts {
  bool showKeyboardEvents;
  bool showPointerEvents;
  bool showPointerMoveEvents;
  bool showTouchEvents;
  bool freeze;
};

struct DebugUIParams {
  DebugUIOpts &opts;
  // A list of all registered input handlers/layers
  const Layer *layers;
  size_t numLayers;
  // The list of events
  TaggedEvent *events;
  size_t numEvents;
  // Set this to clear events
  bool clearEvents;
  size_t currentFrame;
};

} // namespace shards::input::debug

#ifndef RUST_BINDGEN
#include <shards/input/event_buffer.hpp>
namespace shards::input::debug {

struct IDebug {
  virtual const char *getDebugName() = 0;
};
} // namespace shards::input::debug
extern "C" {
struct SHVar;
void shards_input_showDebugUI(SHVar *context, shards::input::debug::DebugUIParams &params);
}
#else
extern "C" {
struct SHVar;
const char *shards_input_eventToString(shards::input::debug::OpaqueEvent);
void shards_input_freeString(const char *str);
bool shards_input_eventIsConsumed(shards::input::debug::OpaqueEvent);
void* shards_input_eventConsumedBy(shards::input::debug::OpaqueEvent);
size_t shards_input_eventType(shards::input::debug::OpaqueEvent);
shards::input::debug::OpaqueLayer shards_input_eventConsumedBy(shards::input::debug::OpaqueEvent);
const char* shards_input_layerName(shards::input::debug::OpaqueLayer);
}
#endif
#endif /* D9559C5B_F728_4B73_B3D2_372D625A794D */
