
#include "debugger_data.hpp"

namespace gfx::debug {
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
  fq.frames = renderGraph.frames;
  fq.outputs = renderGraph.outputs;
  for (auto &node : renderGraph.nodes) {
    auto &newNode = fq.nodes.emplace_back();
    newNode.renderTargetLayout = node.renderTargetLayout;
    newNode.queueDataIndex = node.queueDataIndex;
    newNode.readsFrom = node.readsFrom;
    newNode.writesTo = node.writesTo;
  }
}
void Debugger::frameQueueRenderGraphNodeBegin(size_t index) {
  _impl->currentWriteNode = index;
  // auto &fq = _impl->writeLog->frameQueues.back();
  // auto& node = fq.nodes.emplace_back();
}
void Debugger::frameQueueRenderGraphNodeEnd() {
  // auto &fq = _impl->writeLog->frameQueues.back();
}
void Debugger::pipelineGroupBegin(PipelineGroupDesc desc) {
  auto &fq = _impl->writeLog->frameQueues.back();
  auto &node = fq.nodes[_impl->currentWriteNode];
  auto &group = node.pipelineGroups.emplace_back();
  group.pipeline = std::move(desc.pipeline);
  group.drawables = std::move(desc.drawables);
}
void Debugger::referenceDrawable(std::shared_ptr<IDrawable> drawable) {
  auto id = drawable->getId();
  auto &log = _impl->writeLog;
  log->drawables.emplace(id, drawable);
}
TexturePtr Debugger::getDebugTexture() {
}
} // namespace gfx::debug