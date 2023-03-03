#ifndef FBBE103B_4B4A_462F_BE67_FE59A0C30FB5
#define FBBE103B_4B4A_462F_BE67_FE59A0C30FB5

#include "boost/container/stable_vector.hpp"
#include "drawable.hpp"
#include "fwd.hpp"
#include "render_target.hpp"
#include "renderer_types.hpp"
#include "texture.hpp"
#include "texture_cache.hpp"
#include "fmt.hpp"
#include "worker_memory.hpp"
#include "pmr/unordered_set.hpp"
#include "pmr/vector.hpp"
#include "gfx_wgpu.hpp"
#include "drawable_processor.hpp"
#include <compare>
#include <span>

namespace gfx::detail {

struct RenderTargetFormat {
  WGPUTextureFormat format;
  int2 size;

  std::strong_ordering operator<=>(const RenderTargetFormat &other) const = default;
};

// Caches render target textures based on format/size
struct RenderTextureCache {
  struct Item {
    TexturePtr handle;
    size_t lastTouched;
  };

  struct Bin {
    RenderTargetFormat format;
    std::vector<Item> items;
    std::vector<size_t> freeList;

    void reset() {
      freeList.clear();
      for (size_t i = 0; i < items.size(); i++) {
        freeList.push_back(i);
      }
    }

    // return true if texture is already initialized
    //  false if it's an uninitialized texture
    bool getItem(TexturePtr *&outTexture, size_t frameCounter) {
      if (freeList.size() == 0) {
        Item &newItem = items.emplace_back();

        newItem.lastTouched = frameCounter;
        outTexture = &newItem.handle;
        return false;
      } else {
        size_t index = freeList.back();
        freeList.pop_back();
        Item &item = items[index];

        item.lastTouched = frameCounter;
        outTexture = &item.handle;
        return true;
      }
    }
  };

  std::map<RenderTargetFormat, Bin> bins;

  // Cleanup old render targets
  void resetAndClearOldCacheItems(size_t frameCount, size_t frameThreshold) {
    for (auto it = bins.begin(); it != bins.end();) {
      auto &bin = it->second;
      if (cleanupBin(bin, frameCount, frameThreshold) == 0) {
        // Evict
        it = bins.erase(it);
      } else {
        // readd items to free list
        bin.reset();
        ++it;
      }
    }
  }

  const TexturePtr &allocate(const RenderTargetFormat &format, size_t frameCounter) {
    auto it = bins.find(format);
    if (it == bins.end())
      it = bins.insert_or_assign(format, Bin{.format = format}).first;

    auto &bin = it->second;

    TexturePtr *texture;
    if (!bin.getItem(texture, frameCounter)) {
      (*texture) = std::make_shared<Texture>();
      (*texture)
          ->init(TextureDesc{
              .format =
                  TextureFormat{
                      .dimension = TextureDimension::D2,
                      .flags = TextureFormatFlags::RenderAttachment,
                      .pixelFormat = format.format,
                  },
              .resolution = format.size,
          })
          .initWithLabel("auto");
    }

    return *texture;
  }

private:
  static size_t cleanupBin(Bin &bin, size_t frameCounter, size_t frameThreshold) {
    for (auto it = bin.items.begin(); it != bin.items.end();) {
      int64_t delta = int64_t(frameCounter) - int64_t(it->lastTouched);
      if (delta > int64_t(frameThreshold)) {
        it = bin.items.erase(it);
      } else {
        ++it;
      }
    }
    return bin.items.size();
  }
};

struct RenderGraph;
struct RenderGraphNode;
struct RenderGraphEvaluator;
struct RenderGraphEncodeContext {
  WGPURenderPassEncoder encoder;
  const ViewData &viewData;
  RenderGraphEvaluator &evaluator;
  const RenderGraph &graph;
  const RenderGraphNode &node;
};

typedef size_t FrameIndex;

struct RenderGraphNode {
  struct Attachment {
    FrameIndex frameIndex;
    std::optional<ClearValues> clearValues;

    Attachment(FrameIndex frameIndex) : frameIndex(frameIndex) {}
    Attachment(FrameIndex frameIndex, const ClearValues &clearValues) : frameIndex(frameIndex), clearValues(clearValues) {}

    operator FrameIndex() const { return frameIndex; }
  };

  RenderTargetLayout renderTargetLayout;
  std::vector<Attachment> writesTo;
  std::vector<FrameIndex> readsFrom;

  // Sets up the render pass
  std::function<void(WGPURenderPassDescriptor &)> setupPass;

  // Writes the render pass
  std::function<void(RenderGraphEncodeContext &)> encode;

  RenderGraphNode() = default;
  RenderGraphNode(RenderGraphNode &&) = default;
  RenderGraphNode &operator=(RenderGraphNode &&) = default;
};

// Keeps a list of nodes that contain GPU commands
struct RenderGraph {
  struct Frame {
    std::string name;
    int2 size;
    WGPUTextureFormat format;
    std::optional<size_t> outputIndex;
    TextureSubResource textureOverride;
  };

  struct Output {
    std::string name;
    FrameIndex frameIndex;
  };

  std::vector<RenderGraphNode> nodes;
  std::vector<Frame> frames;
  std::vector<Output> outputs;
};

struct RenderGraphBuilder {
  // Temporary data about nodes
  struct Attachment {
    RenderStepOutput::OutputVariant output;
    FrameIndex frameIndex;
    int2 targetSize;
  };

  struct NodeBuildData {
    ViewData viewData;
    PipelineStepPtr step;
    // This points to which data slot to use when resolving view data
    size_t queueDataIndex;

    std::vector<FrameIndex> readsFrom;
    std::vector<Attachment> attachments;
    bool forceOverwrite{};

    NodeBuildData(const ViewData &viewData, PipelineStepPtr step, size_t queueDataIndex)
        : viewData(viewData), step(step), queueDataIndex(queueDataIndex) {}
  };

  // boost::container::stable_vector<RenderGraphNode> nodes;
  boost::container::stable_vector<NodeBuildData> buildingNodes;

  // std::vector<RenderGraphNode> nodes;
  // std::vector<NodeBuildData> nodeBuildData;
  std::vector<RenderGraph::Frame> frames;
  std::vector<RenderGraph::Output> outputs;
  std::map<std::string, FrameIndex> nameLookup;
  std::map<TextureSubResource, FrameIndex> handleLookup;
  std::set<FrameIndex> externallyWrittenTo;
  float2 referenceOutputSize;

  // Adds a node to the render graph
  void addNode(const ViewData &viewData, const PipelineStepPtr &step, size_t queueDataIndex);
  // Allocate outputs for a render graph node
  void allocateOutputs(NodeBuildData &buildData, const RenderStepOutput &output, bool forceOverwrite = false);
  // Allocate outputs for a render graph node
  void allocateInputs(NodeBuildData &buildData, const std::vector<std::string> &inputs);

  // Get or allocate a frame based on it's description
  FrameIndex assignFrame(const RenderStepOutput::OutputVariant &output, int2 targetSize);

  // Find the index of a frame
  bool findFrameIndex(const std::string &name, FrameIndex &outFrameIndex);
};

struct DrawableProcessorCache {
  Context &context;
  std::shared_mutex drawableProcessorsMutex;
  std::map<DrawableProcessorConstructor, std::shared_ptr<IDrawableProcessor>> drawableProcessors;

  DrawableProcessorCache(Context &context) : context(context) {}

  IDrawableProcessor &get(DrawableProcessorConstructor constructor) {
    drawableProcessorsMutex.lock();
    auto it = drawableProcessors.find(constructor);
    if (it == drawableProcessors.end()) {
      it = drawableProcessors.emplace(constructor, constructor(context)).first;
    }
    drawableProcessorsMutex.unlock();
    return *it->second.get();
  }

  void beginFrame(size_t frameCounter) {
    for (auto &drawableProcessor : drawableProcessors) {
      drawableProcessor.second->reset(frameCounter);
    }
  }
};

struct RendererStorage;
struct RenderGraphEvaluator {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

private:
  allocator_type allocator;

  // Keeps track of texture that have been written to at least once
  shards::pmr::unordered_set<Texture *> writtenTextures;

  // Resolved texture handles for indexed by frame index inside nodes
  struct ResolvedFrameTexture {
    TexturePtr texture;
    WGPUTextureView view{};

    ResolvedFrameTexture(TexturePtr texture, WGPUTextureView view) : texture(texture), view(view) {}
  };
  shards::pmr::vector<ResolvedFrameTexture> frameTextures;

  Renderer &renderer;
  RendererStorage &storage;

public:
  RenderGraphEvaluator(allocator_type allocator, Renderer &renderer, RendererStorage &storage);

  RendererStorage &getStorage() { return storage; }

  Renderer &getRenderer() { return renderer; }

  bool isWrittenTo(const TexturePtr &texture) const { return writtenTextures.contains(texture.get()); }

  const TexturePtr &getTexture(FrameIndex frameIndex) {
    if (frameIndex >= frameTextures.size())
      throw std::out_of_range("frameIndex");
    return frameTextures[frameIndex].texture;
  }

  // Describes a render pass so that targets are cleared to their default value or loaded
  // based on if they were already written to previously
  WGPURenderPassDescriptor createRenderPassDescriptor(const RenderGraph &graph, Context &context, const RenderGraphNode &node,
                                                      size_t frameCounter) {
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

  // Reset all render texture write state
  void reset(size_t frameCounter) {
    writtenTextures.clear();
    frameTextures.clear();
  }

  void getFrameTextures(shards::pmr::vector<ResolvedFrameTexture> &outFrameTextures, const std::span<TextureSubResource> &outputs,
                        const RenderGraph &graph, Context &context, size_t frameCounter);

  void evaluate(const RenderGraph &graph, const ViewData &viewData, std::span<TextureSubResource> outputs, Context &context,
                size_t frameCounter) {
    // Evaluate all render steps in the order they are defined
    // potential optimizations:
    // - evaluate steps without dependencies in parralel
    // - group multiple steps into single render passes

    // Allocate frame textures
    getFrameTextures(frameTextures, outputs, graph, context, frameCounter);

    WGPUCommandEncoderDescriptor desc = {};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(context.wgpuDevice, &desc);

    for (const auto &node : graph.nodes) {
      WGPURenderPassDescriptor renderPassDesc = createRenderPassDescriptor(graph, context, node, frameCounter);
      if (node.setupPass)
        node.setupPass(renderPassDesc);
      WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
      RenderGraphEncodeContext ctx{
          .encoder = renderPassEncoder,
          .viewData = viewData,
          .evaluator = *this,
          .graph = graph,
          .node = node,
      };
      if (node.encode)
        node.encode(ctx);
      wgpuRenderPassEncoderEnd(renderPassEncoder);
    }

    WGPUCommandBufferDescriptor cmdBufDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

    context.submit(cmdBuf);
  }
};

struct CachedRenderGraph {
  RenderGraph renderGraph;

  RenderGraphNode &getNode(size_t index) { return renderGraph.nodes[index]; }
};
typedef std::shared_ptr<CachedRenderGraph> CachedRenderGraphPtr;

struct RenderGraphCache {
  std::unordered_map<Hash128, CachedRenderGraphPtr> map;

  void clear() { map.clear(); }
};

} // namespace gfx::detail

#endif /* FBBE103B_4B4A_462F_BE67_FE59A0C30FB5 */
