#ifndef FBBE103B_4B4A_462F_BE67_FE59A0C30FB5
#define FBBE103B_4B4A_462F_BE67_FE59A0C30FB5

#include "fwd.hpp"
#include "render_target.hpp"
#include "renderer_types.hpp"
#include "texture.hpp"
#include "texture_cache.hpp"
#include "fmt.hpp"
#include "worker_memory.hpp"
#include "pmr/set.hpp"
#include "pmr/vector.hpp"
#include <compare>
#include <span>
#include <webgpu-headers/webgpu.h>

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
                      .type = TextureDimension::D2,
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
    std::vector<Attachment> attachments;
    bool forceOverwrite{};
  };
  std::vector<RenderGraphNode> nodes;
  std::vector<NodeBuildData> nodeBuildData;
  std::vector<RenderGraph::Frame> frames;
  std::vector<RenderGraph::Output> outputs;
  std::map<std::string, FrameIndex> nameLookup;
  std::map<TextureSubResource, FrameIndex> handleLookup;
  std::set<FrameIndex> externallyWrittenTo;
  float2 referenceOutputSize;

  void setReferenceOutputSize(int2 size) { referenceOutputSize = float2(size); }

  bool findFrameIndex(const std::string &name, FrameIndex &outFrameIndex) {
    auto it = nameLookup.find(name);
    if (it == nameLookup.end())
      return false;
    outFrameIndex = it->second;
    return true;
  }

  FrameIndex assignFrame(const RenderStepOutput::OutputVariant &output, int2 targetSize) {
    // Try to find existing frame that matches output
    FrameIndex frameIndex = ~0;
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;

          if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
            auto it0 = handleLookup.find(arg.subResource);
            if (it0 == handleLookup.end()) {
              frameIndex = frames.size();
              it0 = handleLookup.insert_or_assign(arg.subResource, frameIndex).first;
            } else {
              frameIndex = it0->second;
            }
          }

          auto it1 = nameLookup.find(arg.name);
          if (it1 == nameLookup.end()) {
            if (frameIndex == size_t(~0))
              frameIndex = frames.size();
            it1 = nameLookup.insert_or_assign(arg.name, frameIndex).first;
          } else {
            frameIndex = it1->second;
          }
        },
        output);

    // Create a new frame
    assert(frameIndex != size_t(~0));
    if (frameIndex >= frames.size()) {
      auto &frame = frames.emplace_back();
      frame.size = targetSize;
      std::visit(
          [&](auto &&arg) {
            frame.name = arg.name;
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
              assert(arg.subResource);
              frame.textureOverride = arg.subResource;
              frame.format = frame.textureOverride.texture->getFormat().pixelFormat;
            } else if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
              frame.format = arg.format;
            }
          },
          output);
    }

    return frameIndex;
  }

  void checkNoReadFrom(size_t startNodeIndex, FrameIndex frameIndex) {
    // Make sure no node after this references this as an input anymore
    for (size_t i1 = startNodeIndex; i1 < nodes.size(); i1++) {
      auto &node = nodes[i1];
      if (std::find(node.readsFrom.begin(), node.readsFrom.end(), frameIndex) != node.readsFrom.end())
        throw std::logic_error("Cannot attach graph output");
    }
  }

  // Updates nodes that write to given frame to write to the new frame instead
  void replaceFrame(FrameIndex index, FrameIndex newIndex) {
    for (size_t i = 0; i < nodes.size(); i++) {
      auto &node = nodes[i];
      for (auto &attachment : node.writesTo) {
        if (attachment.frameIndex == index)
          attachment.frameIndex = newIndex;
      }
    }

    if (externallyWrittenTo.contains(index)) {
      externallyWrittenTo.erase(index);
      externallyWrittenTo.insert(newIndex);
    }
  }

  void attachOutput(const std::string &name, TexturePtr texture) {
    WGPUTextureFormat outputTextureFormat = texture->getFormat().pixelFormat;

    // Go in reverse, and replace the last write to the given name to this texture
    for (size_t ir = 0; ir < nodes.size(); ir++) {
      size_t i = nodes.size() - 1 - ir;
      auto &node = nodes[i];
      for (auto &attachment : node.writesTo) {
        auto &frame = frames[attachment.frameIndex];
        if (frame.name == name) {
          FrameIndex outputFrameIndex = frames.size();
          size_t outputIndex = outputs.size();
          outputs.emplace_back(RenderGraph::Output{
              .name = name,
              .frameIndex = outputFrameIndex,
          });

          FrameIndex prevFrameIndex = attachment.frameIndex;
          frames.emplace_back(RenderGraph::Frame{
              .name = name,
              .size = frame.size,
              .format = outputTextureFormat,
              .outputIndex = outputIndex,
          });

          // Make sure no node after this references this as an input anymore
          // TODO: support this case by forcing a copy from intermediate texture to output
          checkNoReadFrom(i + 1, prevFrameIndex);

          replaceFrame(prevFrameIndex, outputFrameIndex);

          return;
        }
      }
    }
  }

  // Call before allocating a node's outputs
  void allocateInputs(size_t nodeIndex, const std::vector<std::string> &inputs) {
    nodes.resize(std::max(nodes.size(), nodeIndex + 1));
    RenderGraphNode &node = nodes[nodeIndex];

    for (auto &inputName : inputs) {
      FrameIndex frameIndex{};
      if (findFrameIndex(inputName, frameIndex)) {
        node.readsFrom.push_back(frameIndex);
      }
    }
  }

  bool isWrittenTo(FrameIndex frameIndex, size_t beforeNodeIndex) {
    if (externallyWrittenTo.contains(frameIndex))
      return true;
    for (size_t i = 0; i < beforeNodeIndex; i++) {
      for (auto &attachment : nodes[i].writesTo) {
        if (attachment.frameIndex == frameIndex)
          return true;
      }
    }
    return false;
  }

  void finalizeNodeConnections() {
    static auto logger = gfx::getLogger();

    for (size_t nodeIndex = 0; nodeIndex < nodeBuildData.size(); nodeIndex++) {
      RenderGraphNode &node = nodes[nodeIndex];
      NodeBuildData &nodeBuildData = this->nodeBuildData[nodeIndex];

      for (auto &attachment : nodeBuildData.attachments) {
        FrameIndex frameIndex = attachment.frameIndex;

        WGPUTextureFormat targetFormat{};
        std::optional<ClearValues> clearValues{};
        std::visit(
            [&](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              clearValues = arg.clearValues;
              if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
                targetFormat = arg.format;
              } else if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
                targetFormat = arg.subResource.texture->getFormat().pixelFormat;
              } else {
                throw std::logic_error("");
              }
            },
            attachment.output);

        bool sizeMismatch = frames[frameIndex].size != attachment.targetSize;
        bool formatMismatch = frames[frameIndex].format != targetFormat;

        bool loadRequired = !clearValues.has_value();
        if (nodeBuildData.forceOverwrite) {
          loadRequired = false;
        }

        // Check if the same frame is being read from
        auto it = std::find(node.readsFrom.begin(), node.readsFrom.end(), frameIndex);
        if (sizeMismatch || formatMismatch || it != node.readsFrom.end()) {
          // TODO: Implement copy into loaded attachment from previous step
          if (loadRequired) {
            std::string err = "Copy required, not implemented";
            if (sizeMismatch)
              err += fmt::format(" (size {}=>{})", frames[frameIndex].size, attachment.targetSize);
            if (formatMismatch)
              err += fmt::format(" (fmt {}=>{})", magic_enum::enum_name(frames[frameIndex].format),
                                 magic_enum::enum_name(targetFormat));
            throw std::logic_error(err);
          }

          // Just duplicate the frame for now
          size_t newFrameIndex = frames.size();
          auto &frame = frames.emplace_back(RenderGraph::Frame{
              .name = frames[frameIndex].name,
              .size = attachment.targetSize,
              .format = targetFormat,
          });
          nameLookup[frame.name] = newFrameIndex;
          frameIndex = newFrameIndex;
        }

        // Check if written, force clear with default values if not
        bool needsClearValue = false;
        if (nodeBuildData.forceOverwrite)
          needsClearValue = true;
        else if (loadRequired && !isWrittenTo(frameIndex, nodeIndex)) {
          SPDLOG_LOGGER_DEBUG(
              logger, "Forcing clear with default values for frame {} accessed by node {}, since it's not written to before",
              frameIndex, nodeIndex);
          needsClearValue = true;
        }

        // Set default clear value based on format
        if (needsClearValue) {
          auto &formatDesc = getTextureFormatDescription(targetFormat);
          if (hasAnyTextureFormatUsage(formatDesc.usage, TextureFormatUsage::Depth | TextureFormatUsage::Stencil)) {
            clearValues = ClearValues::getDefaultDepthStencil();
          } else {
            clearValues = ClearValues::getDefaultColor();
          }
        }

        if (clearValues.has_value())
          node.writesTo.emplace_back(frameIndex, clearValues.value());
        else {
          node.writesTo.emplace_back(frameIndex);
        }
      }
    }
  }

  // When setting forceOverwrite, ignores clear value, marks this attachment as being completely overwritten
  // e.g. fullscreen effect passes
  void allocateOutputs(size_t nodeIndex, const RenderStepOutput &output, bool forceOverwrite = false) {
    static auto logger = gfx::getLogger();

    nodes.resize(std::max(nodes.size(), nodeIndex + 1));

    this->nodeBuildData.resize(std::max(this->nodeBuildData.size(), nodeIndex + 1));
    NodeBuildData &nodeBuildData = this->nodeBuildData[nodeIndex];
    nodeBuildData.forceOverwrite = forceOverwrite;

    for (auto &attachment : output.attachments) {
      int2 targetSize = int2(linalg::floor(referenceOutputSize * output.sizeScale.value_or(float2(1.0f))));

      FrameIndex frameIndex = assignFrame(attachment, targetSize);

      nodeBuildData.attachments.push_back(Attachment{attachment, frameIndex, targetSize});
    }
  }

  RenderTargetLayout getLayout(size_t nodeIndex) const {
    if (nodeIndex >= nodes.size())
      throw std::out_of_range("nodeIndex");

    RenderTargetLayout layout{};

    const auto &node = nodes[nodeIndex];
    for (auto &nodeIndex : node.writesTo) {
      auto &frame = frames[nodeIndex];

      std::string name = frame.name;
      if (name == "depth") {
        layout.depthTargetIndex = layout.targets.size();
      }

      layout.targets.emplace_back(RenderTargetLayout::Target{
          .name = std::move(name),
          .format = frame.format,
      });
    }

    return layout;
  }

  RenderGraph finalize() {
    updateNodeLayouts();
    validateNodes();

    RenderGraph graph{
        .nodes = std::move(nodes),
        .frames = std::move(frames),
        .outputs = std::move(outputs),
    };
    return graph;
  }

private:
  void updateNodeLayouts() {
    size_t nodeIndex{};
    for (auto &node : nodes) {
      node.renderTargetLayout = getLayout(nodeIndex);
      nodeIndex++;
    }
  }

  void validateNodes() {
    static auto logger = gfx::getLogger();

    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++) {
      auto &node = nodes[nodeIndex];
      if (node.writesTo.size() == 0) {
        throw std::runtime_error(fmt::format("Node {} does not write to any outputs", nodeIndex));
      }
    }
  }
};

struct RenderGraphEvaluator {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

private:
  allocator_type allocator;

  // Keeps track of texture that have been written to at least once
  shards::pmr::set<Texture *> writtenTextures;

  // Resolved texture handles for indexed by frame index inside nodes
  struct ResolvedFrameTexture {
    TexturePtr texture;
    WGPUTextureView view{};

    ResolvedFrameTexture(TexturePtr texture, WGPUTextureView view) : texture(texture), view(view) {}
  };
  shards::pmr::vector<ResolvedFrameTexture> frameTextures;

  RenderTextureCache &renderTextureCache;
  TextureViewCache &textureViewCache;

public:
  RenderGraphEvaluator(allocator_type allocator, RenderTextureCache &renderTextureCache, TextureViewCache &textureViewCache)
      : allocator(allocator), writtenTextures(allocator), frameTextures(allocator), renderTextureCache(renderTextureCache),
        textureViewCache(textureViewCache) {}

  bool isWrittenTo(const TexturePtr &texture) const { return writtenTextures.contains(texture.get()); }

  const TexturePtr &getTexture(FrameIndex frameIndex) {
    if (frameIndex >= frameTextures.size())
      throw std::out_of_range("frameIndex");
    return frameTextures[frameIndex].texture;
  }

  WGPUTextureView getTextureView(const TextureContextData &textureData, uint8_t faceIndex, uint8_t mipIndex, size_t frameCounter) {
    if (textureData.externalView)
      return textureData.externalView;
    TextureViewDesc desc{
        .format = textureData.format.pixelFormat,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = mipIndex,
        .mipLevelCount = 1,
        .baseArrayLayer = uint32_t(faceIndex),
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    };
    return textureViewCache.getTextureView(frameCounter, textureData.texture, desc);
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
                        const RenderGraph &graph, Context &context, size_t frameCounter) {
    outFrameTextures.clear();
    outFrameTextures.reserve(graph.frames.size());

    // Entire texture view
    auto resolve = [&](TexturePtr texture) {
      return ResolvedFrameTexture(texture, getTextureView(texture->createContextDataConditional(context), 0, 0, frameCounter));
    };

    // Subresource texture view
    auto resolveSubResourceView = [&](const TextureSubResource &resource) {
      return ResolvedFrameTexture(resource.texture, getTextureView(resource.texture->createContextDataConditional(context),
                                                                   resource.faceIndex, resource.mipIndex, frameCounter));
    };

    for (auto &frame : graph.frames) {
      if (frame.outputIndex.has_value()) {
        size_t outputIndex = frame.outputIndex.value();
        if (outputIndex >= outputs.size())
          throw std::logic_error("Missing output");

        outFrameTextures.push_back(resolveSubResourceView(outputs[outputIndex]));
      } else if (frame.textureOverride) {
        outFrameTextures.push_back(resolveSubResourceView(frame.textureOverride));
      } else {
        TexturePtr texture =
            renderTextureCache.allocate(RenderTargetFormat{.format = frame.format, .size = frame.size}, frameCounter);
        outFrameTextures.push_back(resolve(texture));
      }
    }
  }

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
