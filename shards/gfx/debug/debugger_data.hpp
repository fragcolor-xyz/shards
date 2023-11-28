#ifndef DFF4306E_B56B_47D6_BB34_8EE7A39DF8B3
#define DFF4306E_B56B_47D6_BB34_8EE7A39DF8B3

#include "debugger.hpp"
#include "../drawable.hpp"
#include "../renderer_types.hpp"
#include "../renderer_storage.hpp"

namespace gfx {
namespace detail {
template <> struct SizedItemOps<TexturePtr> {
  size_t getCapacity(TexturePtr &item) const { return 0; }
  void init(TexturePtr &item, size_t size) { item = std::make_shared<Texture>(); }
};
} // namespace detail
} // namespace gfx
namespace gfx::debug {
struct Node {
  using Attachment = detail::RenderGraphNode::Attachment;

  detail::RenderTargetLayout renderTargetLayout;
  std::vector<Attachment> writesTo;
  std::vector<detail::FrameIndex> readsFrom;

  // This points to which data slot to use when resolving view data
  size_t queueDataIndex;

  std::vector<PipelineGroupDesc> pipelineGroups;
};

struct FrameQueue {
  using Frame = detail::RenderGraph::Frame;
  using Output = detail::RenderGraph::Output;
  std::vector<Frame> frames;
  std::vector<Output> outputs;
  std::vector<Node> nodes;
};

struct Log {
  size_t frameCounter = 0;
  std::vector<FrameQueue> frameQueues;
  std::unordered_map<UniqueId, DrawablePtr> drawables;
};

struct DebuggerImpl {
  std::shared_mutex mutex;
  std::shared_ptr<Log> log;
  std::shared_ptr<Log> writeLog;
  size_t currentWriteNode;

  detail::Swappable<detail::SizedItemPool<TexturePtr>, 4> texturePools;
  size_t texturePoolIndex;

  std::shared_ptr<Log> getLog() {
    std::shared_lock<std::shared_mutex> l(mutex);
    return log;
  }

  TexturePtr allocateTexture() {
    texturePoolIndex = gfx::mod<int32_t>(texturePoolIndex+1, decltype(texturePools)::MaxSize);
  }
};
} // namespace gfx::debug

#endif /* DFF4306E_B56B_47D6_BB34_8EE7A39DF8B3 */
