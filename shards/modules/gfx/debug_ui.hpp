#ifndef B28AA5FD_3B79_40B7_B746_54141B3A3FDE
#define B28AA5FD_3B79_40B7_B746_54141B3A3FDE

#include <stdint.h>
#include <stddef.h>

namespace shards::gfx::debug {
struct FrameQueue {
  int priority;
  const char *name;
  bool hasFocus{};
};

// Persistent
struct DebugUIOpts {};

struct DebugUIParams {
  DebugUIOpts &opts;
  // A list of all render calls in the current frame
  const FrameQueue *frameQueues;
  size_t numFrameQueues;
  size_t currentFrame;
};
} // namespace shards::gfx::debug

#ifndef RUST_BINDGEN
extern "C" {
struct SHVar;
void shards_gfx_showDebugUI(SHVar *context, shards::gfx::debug::DebugUIParams &params);
}
#else
extern "C" {
struct SHVar;
void shards_gfx_freeString(const char *str);
}
#endif

#endif /* B28AA5FD_3B79_40B7_B746_54141B3A3FDE */
