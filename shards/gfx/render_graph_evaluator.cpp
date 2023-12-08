#include "render_graph.hpp"
#include "renderer_storage.hpp"
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <boost/container/flat_map.hpp>

namespace gfx::detail {

RenderGraphEvaluator::RenderGraphEvaluator(allocator_type allocator, Renderer &renderer, RendererStorage &storage)
    : allocator(allocator), writtenTextures(allocator), frameTextures(allocator), renderer(renderer), storage(storage) {}

void RenderGraphEvaluator::getFrameTextures(shards::pmr::vector<ResolvedFrameTexture> &outFrameTextures,
                                            const std::span<TextureSubResource> &outputs, const RenderGraph &graph,
                                            Context &context) {
  outFrameTextures.clear();
  outFrameTextures.reserve(graph.frames.size());

  // Entire texture view
  auto resolve = [&](TexturePtr texture) {
    return ResolvedFrameTexture(texture, storage.getTextureView(texture->createContextDataConditional(context), 0, 0));
  };

  // Subresource texture view
  auto resolveSubResourceView = [&](const TextureSubResource &resource) {
    return ResolvedFrameTexture(resource.texture, storage.getTextureView(resource.texture->createContextDataConditional(context),
                                                                         resource.faceIndex, resource.mipIndex));
  };

  for (auto &frame : graph.frames) {
    std::visit(
        [](auto &binding) {
          using T = std::decay_t<decltype(binding)>;
          if constexpr (std::is_same_v<T, RenderGraph::OutputIndex>) {
          } else if constexpr (std::is_same_v<T, RenderGraph::TextureOverrideRef>) {
            // auto& step = this-> binding.stepIndex
          }
        },
        frame.binding);
    // if (frame.outputIndex.has_value()) {
    //   size_t outputIndex = frame.outputIndex.value();
    //   if (outputIndex >= outputs.size())
    //     throw std::logic_error("Missing output");

    //   outFrameTextures.push_back(resolveSubResourceView(outputs[outputIndex]));
    // } else if (frame.textureOverride) {
    //   // Update the texture size/format
    //   auto desc = frame.textureOverride.texture->getDesc();
    //   desc.resolution = frame.size;
    //   desc.format.pixelFormat = frame.format;
    //   frame.textureOverride.texture->init(desc);
    //   outFrameTextures.push_back(resolveSubResourceView(frame.textureOverride));
    // } else {
    //   TexturePtr texture = storage.renderTextureCache.allocate(RenderTargetFormat{.format = frame.format, .size = frame.size},
    //                                                            storage.frameCounter);
    //   outFrameTextures.push_back(resolve(texture));
    // }
  }
}

WGPURenderPassDescriptor RenderGraphEvaluator::createRenderPassDescriptor(const RenderGraph &graph, Context &context,
                                                                          const RenderGraphNode &node) {
  // NOTE: Allocated in frame memory
  WGPURenderPassDepthStencilAttachment *depthStencilAttachmentPtr{};
  auto &colorAttachments = *allocator.new_object<shards::pmr::vector<WGPURenderPassColorAttachment>>();

  size_t depthIndex = node.renderTargetLayout.depthTargetIndex.value_or(~0);
  for (size_t i = 0; i < node.writesTo.size(); i++) {
    const RenderGraphNode::Attachment &nodeAttachment = node.writesTo[i];
    ResolvedFrameTexture &resolvedFrameTexture = frameTextures[nodeAttachment.frameIndex];

    if (i == depthIndex) {
      auto &attachment = *(depthStencilAttachmentPtr = allocator.new_object<WGPURenderPassDepthStencilAttachment>());
      attachment = WGPURenderPassDepthStencilAttachment{
          .view = resolvedFrameTexture.view,
      };

      if (nodeAttachment.clearValues) {
        auto &clearDepth = nodeAttachment.clearValues.value().depth;
        attachment.depthLoadOp = WGPULoadOp_Clear;
        attachment.depthClearValue = clearDepth;
      } else {
        attachment.depthLoadOp = WGPULoadOp_Load;
      }
      attachment.depthStoreOp = WGPUStoreOp_Store;

      // TODO: Stencil
      uint32_t clearStencil = 0;
      attachment.stencilClearValue = clearStencil;
      attachment.stencilLoadOp = WGPULoadOp_Undefined;
      attachment.stencilStoreOp = WGPUStoreOp_Undefined;
    } else {
      auto &attachment = colorAttachments.emplace_back(WGPURenderPassColorAttachment{
          .view = resolvedFrameTexture.view,
      });

      if (nodeAttachment.clearValues) {
        auto &clearColor = nodeAttachment.clearValues.value().color;
        attachment.loadOp = WGPULoadOp_Clear;
        attachment.clearValue = WGPUColor{clearColor.x, clearColor.y, clearColor.z, clearColor.w};
      } else {
        attachment.loadOp = WGPULoadOp_Load;
      }
      attachment.storeOp = WGPUStoreOp_Store;
    }

    writtenTextures.insert(resolvedFrameTexture.texture.get());
  }

  return WGPURenderPassDescriptor{
      .colorAttachmentCount = uint32_t(colorAttachments.size()),
      .colorAttachments = colorAttachments.data(),
      .depthStencilAttachment = depthStencilAttachmentPtr,
  };
}

void RenderGraphEvaluator::evaluate(const RenderGraph &graph, IRenderGraphEvaluationData &evaluationData,
                                    std::span<TextureSubResource> outputs) {
  // Evaluate all render steps in the order they are defined
  // potential optimizations:
  // - evaluate steps without dependencies in parralel
  // - group multiple steps into single render passes

  auto &context = renderer.getContext();

  // for (const auto &node : graph.nodes) {
  //   if (node.updateOutputs)
  //     node.updateOutputs(const_cast<RenderGraphNode&>(node));
  // }

  // Allocate frame textures
  getFrameTextures(frameTextures, outputs, graph, context);

  WGPUCommandEncoderDescriptor desc = {};
  WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(context.wgpuDevice, &desc);

  static std::shared_ptr<debug::Debugger> NoDebugger;
  auto &debugger = ignoreInDebugger ? NoDebugger : storage.debugger;

  size_t nodeIdx = 0;
  for (const auto &node : graph.nodes) {
    if (debugger)
      debugger->frameQueueRenderGraphNodeBegin(nodeIdx);

    const ViewData &viewData = evaluationData.getViewData(node.queueDataIndex);

    WGPURenderPassDescriptor renderPassDesc = createRenderPassDescriptor(graph, context, node);
    if (node.setupPass)
      node.setupPass(renderPassDesc);
    WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
    RenderGraphEncodeContext ctx{
        .encoder = renderPassEncoder,
        .viewData = viewData,
        .evaluator = *this,
        .graph = graph,
        .node = node,
        .ignoreInDebugger = ignoreInDebugger,
    };
    if (node.encode)
      node.encode(ctx);
    wgpuRenderPassEncoderEnd(renderPassEncoder);

    if (debugger)
      debugger->frameQueueRenderGraphNodeEnd(*this);
    ++nodeIdx;
  }

  WGPUCommandBufferDescriptor cmdBufDesc{};
  WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

  context.submit(cmdBuf);

  // TODO: move this earlier into the process once rendering is multi-threaded
  for (auto &queue : graph.autoClearQueues) {
    queue->clear();
  }
}

} // namespace gfx::detail
