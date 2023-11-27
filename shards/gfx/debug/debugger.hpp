#ifndef D3FAA94B_8F2B_46E3_9028_3E6A676F7093
#define D3FAA94B_8F2B_46E3_9028_3E6A676F7093

#include "../fwd.hpp"
#include "../types.hpp"
#include "../unique_id.hpp"
#include <vector>

namespace gfx {
namespace detail {
struct RendererStorage;
struct RenderGraph;
struct CachedPipeline;
} // namespace detail

namespace debug {
struct OutputDesc {
  TexturePtr mainOutput;
  RenderTargetPtr mainOutputRT;
};
struct RenderDesc {
  const std::string tag;
};
struct FQDesc {
  Rect viewport;
  RenderTargetPtr renderTarget;
  int2 referenceSize;
};
struct PipelineGroupDesc {
  std::shared_ptr<detail::CachedPipeline> pipeline;
  std::vector<UniqueId> drawables;
};
struct DebuggerImpl;
struct Debugger {
  std::shared_ptr<DebuggerImpl> _impl;

  Debugger();
  void frameBegin(detail::RendererStorage &storage);
  void frameEnd(detail::RendererStorage &storage, const OutputDesc &outputDesc);
  void frameQueuePush(FQDesc desc);
  void frameQueueRenderGraphBegin(const detail::RenderGraph &renderGraph);
  void frameQueueRenderGraphNodeBegin(size_t index);
  void frameQueueRenderGraphNodeEnd();
  void frameQueuePop();
  void pipelineGroupBegin(PipelineGroupDesc desc);
  void referenceDrawable(std::shared_ptr<IDrawable> drawable);
};
} // namespace debug
} // namespace gfx

#endif /* D3FAA94B_8F2B_46E3_9028_3E6A676F7093 */
