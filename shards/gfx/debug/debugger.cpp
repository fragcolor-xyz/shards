#include "debugger.hpp"
#include "../drawable.hpp"
#include "../renderer_types.hpp"
#include "../renderer_storage.hpp"

namespace gfx::debug {

struct FrameQueue {
  detail::RenderGraph renderGraph;
};

struct Log {
  size_t frameCounter = 0;
  std::vector<FrameQueue> frameQueues;
};

struct DebuggerImpl {
  std::shared_mutex mutex;
  std::shared_ptr<Log> log;
  std::shared_ptr<Log> writeLog;
};

Debugger::Debugger() { _impl = std::make_shared<DebuggerImpl>(); }
void Debugger::frameBegin(detail::RendererStorage &storage) {
  assert(!_impl->writeLog);
  _impl->writeLog = std::make_shared<Log>();
  _impl->writeLog->frameCounter = storage.frameCounter;
}
void Debugger::frameEnd(detail::RendererStorage &storage, const OutputDesc &outputDesc) {
  assert(_impl->writeLog);

  // Finalize & swap
  std::unique_lock<std::shared_mutex> l(_impl->mutex);
  std::swap(_impl->log, _impl->writeLog);
  _impl->writeLog.reset();
}
void Debugger::frameQueuePush(FQDesc desc) { _impl->writeLog->frameQueues.emplace_back(); }
void Debugger::frameQueuePop() { _impl->writeLog->frameQueues.emplace_back(); }
void Debugger::frameQueueRenderGraphBegin(const detail::RenderGraph &renderGraph) {
  auto &fq = _impl->writeLog->frameQueues.back();
  fq.renderGraph.frames = renderGraph.frames;
  fq.renderGraph.outputs = renderGraph.outputs;
  for(auto& node : renderGraph.nodes) {
    auto& newNode = fq.renderGraph.nodes.emplace_back();
    newNode.renderTargetLayout = node.renderTargetLayout;
    newNode.queueDataIndex = node.queueDataIndex;
    newNode.readsFrom = node.readsFrom;
    newNode.writesTo = node.writesTo;
  }
}
void Debugger::frameQueueRenderGraphNodeBegin(size_t index) { auto &fq = _impl->writeLog->frameQueues.back(); }
void Debugger::frameQueueRenderGraphNodeEnd() { auto &fq = _impl->writeLog->frameQueues.back(); }
void Debugger::pipelineGroupBegin(PipelineGroupDesc desc) {}
void Debugger::referenceDrawable(std::shared_ptr<IDrawable> drawable) {}
} // namespace gfx::debug