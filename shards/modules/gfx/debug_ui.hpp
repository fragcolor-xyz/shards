#ifndef B28AA5FD_3B79_40B7_B746_54141B3A3FDE
#define B28AA5FD_3B79_40B7_B746_54141B3A3FDE

#include <stdint.h>
#include <stddef.h>

namespace shards::gfx::debug_ui {

struct Int2 {
  int64_t x;
  int64_t y;
};

struct Target {
  const char *name;
};
typedef uint64_t FrameIndex;
struct Frame {
  const char *name;
  Int2 size;
  int32_t format;
  bool isOutput;
};
struct Output {};
struct Drawable {
  uint64_t id;
};
struct PipelineGroup {
  Drawable *drawables;
  size_t numDrawables;
};
struct Node {
  Target *targets;
  size_t numTargets;
  PipelineGroup *pipelineGroups;
  size_t numPipelineGroups;
};
struct FrameQueue {
  Frame *frames;
  size_t numFrames;
  Output *outputs;
  size_t numOutputs;
  Node *nodes;
  size_t numNodes;
};

// Persistent
struct UIOpts {};

struct UIParams {
  void *state;
  UIOpts &opts;
  bool debuggerEnabled;
  // A list of all render calls in the current frame
  FrameQueue *frameQueues;
  size_t numFrameQueues;
  size_t currentFrame;
};
} // namespace shards::gfx::debug_ui

#ifndef RUST_BINDGEN
extern "C" {
struct SHVar;
void *shards_gfx_newDebugUI();
void shards_gfx_freeDebugUI(void *);
void shards_gfx_showDebugUI(SHVar *context, shards::gfx::debug_ui::UIParams &params);
}
#else
extern "C" {
struct SHVar;
void shards_gfx_freeString(const char *str);
}
#endif

#endif /* B28AA5FD_3B79_40B7_B746_54141B3A3FDE */
