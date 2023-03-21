#ifndef D9559C5B_F728_4B73_B3D2_372D625A794D
#define D9559C5B_F728_4B73_B3D2_372D625A794D

#include <stdint.h>

namespace shards::input::debug {
struct Layer {
  bool focused;
  int priority;
  const char *name;
};

struct IDebug {
  virtual const char *getDebugName() = 0;
};
} // namespace shards::input::debug

extern "C" {
struct SHVar;
void showInputDebugUI(SHVar *context, shards::input::debug::Layer *layers, size_t layerCount);
}

#endif /* D9559C5B_F728_4B73_B3D2_372D625A794D */
