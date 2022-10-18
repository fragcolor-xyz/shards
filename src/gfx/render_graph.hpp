#ifndef FBBE103B_4B4A_462F_BE67_FE59A0C30FB5
#define FBBE103B_4B4A_462F_BE67_FE59A0C30FB5

#include "renderer_types.hpp"
#include <compare>

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

  static constexpr size_t cleanupThreshold = 10;

  // Cleanup old render targets
  void swapBuffers(size_t frameCount) {
    for (auto it = bins.begin(); it != bins.end();) {
      auto &bin = it->second;
      if (cleanupBin(bin, frameCount) == 0) {
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
                      .type = TextureType::D2,
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
  static size_t cleanupBin(Bin &bin, size_t frameCounter) {
    for (auto it = bin.items.begin(); it != bin.items.end();) {
      if ((frameCounter - it->lastTouched) > cleanupThreshold) {
        it = bin.items.erase(it);
      } else {
        ++it;
      }
    }
    return bin.items.size();
  }
};

struct RenderTargetData {
  struct DepthTarget {
    Texture *texture;

    bool operator==(const DepthTarget &other) const { return texture == other.texture; }
    bool operator!=(const DepthTarget &other) const { return !(*this == other); }
  };

  struct ColorTarget {
    std::string name;
    Texture *texture;

    bool operator==(const ColorTarget &other) const { return name == other.name && texture == other.texture; }
    bool operator!=(const ColorTarget &other) const { return !(*this == other); }
  };

  std::optional<DepthTarget> depthTarget;
  std::vector<ColorTarget> colorTargets;
  std::vector<TexturePtr> references;

  // Use managed render target with size adjusted to reference size
  void init(RenderTargetPtr renderTarget) {
    for (auto &pair : renderTarget->attachments) {
      auto &attachment = pair.second;
      auto &key = pair.first;
      if (key == "depth") {
        depthTarget = DepthTarget{
            .texture = attachment.get(),
        };
      } else {
        colorTargets.push_back(ColorTarget{
            .name = key,
            .texture = attachment.get(),
        });
      }

      references.push_back(attachment);
    }

    std::sort(colorTargets.begin(), colorTargets.end(),
              [](const ColorTarget &a, const ColorTarget &b) { return a.name < b.name; });
  }

  // Use a device's main output with managed depth buffer
  void init(const Renderer::MainOutput &mainOutput, TexturePtr depthStencilBuffer) {
    references.push_back(depthStencilBuffer);
    depthTarget = DepthTarget{
        .texture = depthStencilBuffer.get(),
    };

    references.push_back(mainOutput.texture);
    colorTargets.push_back(ColorTarget{
        .name = "color",
        .texture = mainOutput.texture.get(),
    });
  }

  bool operator==(const RenderTargetData &other) const {
    if (!std::equal(colorTargets.begin(), colorTargets.end(), other.colorTargets.begin(), other.colorTargets.end()))
      return false;

    if (depthTarget != other.depthTarget)
      return false;

    return true;
  }

  bool operator!=(const RenderTargetData &other) const { return !(*this == other); }
};

struct RenderGraph;
struct RenderGraphNode;
struct RenderGraphEvaluator;
struct EvaluateNodeContext {
  WGPURenderPassEncoder encoder;
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
  std::function<void(EvaluateNodeContext &)> body;

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
    TexturePtr textureOverride;
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
  std::vector<RenderGraph::Frame> frames;
  std::vector<RenderGraph::Output> outputs;
  std::map<std::string, FrameIndex> nameLookup;
  std::map<TexturePtr, FrameIndex> handleLookup;
  float2 referenceOutputSize;

  std::vector<RenderGraphNode> nodes;

  void reset() { nodes.clear(); }

  void setReferenceOutputSize(int2 size) { referenceOutputSize = float2(size); }

  bool findFrameIndex(const std::string &name, FrameIndex &outFrameIndex) {
    auto it = nameLookup.find(name);
    if (it == nameLookup.end())
      return false;
    outFrameIndex = it->second;
    return true;
  }

  FrameIndex assignFrame(const RenderStepOutput::OutputVariant &output, int2 targetSize) {
    FrameIndex frameIndex = ~0;
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;

          if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
            auto it0 = handleLookup.find(arg.handle);
            if (it0 == handleLookup.end()) {
              frameIndex = frames.size();
              it0 = handleLookup.insert_or_assign(arg.handle, frameIndex).first;
            } else {
              frameIndex = it0->second;
            }
          }

          auto it1 = nameLookup.find(arg.name);
          if (it1 == nameLookup.end()) {
            if (frameIndex == ~0)
              frameIndex = frames.size();
            it1 = nameLookup.insert_or_assign(arg.name, frameIndex).first;
          } else {
            frameIndex = it1->second;
          }
        },
        output);

    assert(frameIndex != ~0);
    if (frameIndex >= frames.size()) {
      // Initialize frame info
      auto &frame = frames.emplace_back();
      frame.size = targetSize;
      std::visit(
          [&](auto &&arg) {
            frame.name = arg.name;
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
              assert(arg.handle);
              frame.textureOverride = arg.handle;
              frame.format = frame.textureOverride->getFormat().pixelFormat;
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
    for (size_t i = 0; i < beforeNodeIndex; i++) {
      for (auto &attachment : nodes[i].writesTo) {
        if (attachment.frameIndex == frameIndex)
          return true;
      }
    }
    return false;
  }

  // When setting forceOverwrite, ignores clear value, marks this attachment as being completely overwritten
  // e.g. fullscreen effect passes
  void allocateOutputs(size_t nodeIndex, const RenderStepOutput &output, bool forceOverwrite = false) {
    static auto logger = gfx::getLogger();

    nodes.resize(std::max(nodes.size(), nodeIndex + 1));
    RenderGraphNode &node = nodes[nodeIndex];

    for (auto &attachment : output.attachments) {
      int2 targetSize = int2(linalg::floor(referenceOutputSize * output.sizeScale.value_or(float2(1.0f))));

      FrameIndex frameIndex = assignFrame(attachment, targetSize);

      WGPUTextureFormat targetFormat{};
      std::optional<ClearValues> clearValues{};
      std::visit(
          [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            clearValues = arg.clearValues;
            if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
              targetFormat = arg.format;
            } else if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
              targetFormat = arg.handle->getFormat().pixelFormat;
            } else {
              throw std::logic_error("");
            }
          },
          attachment);

      bool sizeMismatch = frames[frameIndex].size != targetSize;
      bool formatMismatch = frames[frameIndex].format != targetFormat;

      bool loadRequired = !clearValues.has_value();
      if (forceOverwrite) {
        loadRequired = false;
      }

      // Check if the same frame is being read from
      auto it = std::find(node.readsFrom.begin(), node.readsFrom.end(), frameIndex);
      if (sizeMismatch || formatMismatch || it != node.readsFrom.end()) {
        // TODO: Implement copy into loaded attachment from previous step
        if (loadRequired)
          throw std::logic_error("Copy required, not implemented");

        // Just duplicate the frame for now
        size_t newFrameIndex = frames.size();
        auto &frame = frames.emplace_back(RenderGraph::Frame{
            .name = frames[frameIndex].name,
            .size = targetSize,
            .format = targetFormat,
        });
        nameLookup[frame.name] = newFrameIndex;
        frameIndex = newFrameIndex;
      }

      // Check if written, force clear with default values if not
      bool needsClearValue = false;
      if (forceOverwrite)
        needsClearValue = true;
      else if (loadRequired && !isWrittenTo(frameIndex, nodeIndex)) {
        SPDLOG_LOGGER_WARN(logger,
                           "Forcing clear with default values for frame {} accessed by node {}, since it's not written to before",
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

  void updateNodeLayouts() {
    size_t nodeIndex{};
    for (auto &node : nodes) {
      node.renderTargetLayout = getLayout(nodeIndex);
      nodeIndex++;
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

  void validateNodes() {
    static auto logger = gfx::getLogger();

    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++) {
      auto &node = nodes[nodeIndex];
      if (node.writesTo.size() == 0) {
        throw std::runtime_error(fmt::format("Node {} does not write to any outputs", nodeIndex));
      }
    }
  }

  RenderGraph finalize() {
    validateNodes();

    RenderGraph graph{
        .nodes = std::move(nodes),
        .frames = std::move(frames),
        .outputs = std::move(outputs),
    };
    return graph;
  }
};

struct RenderGraphEvaluator {
private:
  // Keeps track of texture that have been written to at least once
  std::set<Texture *> writtenTextures;

  // Resolved texture handles for indexed by frame index inside nodes
  std::vector<TexturePtr> frameTextures;

  std::vector<WGPURenderPassColorAttachment> cachedColorAttachments;
  std::optional<WGPURenderPassDepthStencilAttachment> cachedDepthStencilAttachment;
  WGPURenderPassDescriptor cachedRenderPassDescriptor{};
  RenderTextureCache renderTextureCache;

public:
  bool isWrittenTo(const TexturePtr &texture) const { return writtenTextures.contains(texture.get()); }

  const TexturePtr &getTexture(FrameIndex frameIndex) {
    if (frameIndex >= frameTextures.size())
      throw std::out_of_range("frameIndex");
    return frameTextures[frameIndex];
  }

  // Describes a render pass so that targets are cleared to their default value or loaded
  // based on if they were already written to previously
  WGPURenderPassDescriptor &createRenderPassDescriptor(const RenderGraph &graph, Context &context, const RenderGraphNode &node) {
    cachedColorAttachments.clear();
    cachedDepthStencilAttachment.reset();

    memset(&cachedRenderPassDescriptor, 0, sizeof(cachedRenderPassDescriptor));

    size_t depthIndex = node.renderTargetLayout.depthTargetIndex.value_or(~0);
    for (size_t i = 0; i < node.writesTo.size(); i++) {
      const RenderGraphNode::Attachment &nodeAttachment = node.writesTo[i];
      TexturePtr &texture = frameTextures[nodeAttachment.frameIndex];

      auto &textureData = texture->createContextDataConditional(context);

      if (i == depthIndex) {
        auto &textureData = texture->createContextDataConditional(context);
        cachedDepthStencilAttachment = WGPURenderPassDepthStencilAttachment{
            .view = textureData.defaultView,
        };
        auto &attachment = cachedDepthStencilAttachment.value();

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
        auto &attachment = cachedColorAttachments.emplace_back(WGPURenderPassColorAttachment{
            .view = textureData.defaultView,
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

      writtenTextures.insert(texture.get());
    }

    cachedRenderPassDescriptor.colorAttachments = cachedColorAttachments.data();
    cachedRenderPassDescriptor.colorAttachmentCount = cachedColorAttachments.size();
    cachedRenderPassDescriptor.depthStencilAttachment =
        cachedDepthStencilAttachment ? &cachedDepthStencilAttachment.value() : nullptr;

    return cachedRenderPassDescriptor;
  }

  // Reset all render texture write state
  void reset(size_t frameCounter) {
    writtenTextures.clear();
    frameTextures.clear();
    renderTextureCache.swapBuffers(frameCounter);
  }

  void getFrameTextures(std::vector<TexturePtr> &outFrameTextures, const std::vector<TexturePtr> &outputs,
                        const RenderGraph &graph, size_t frameCounter) {
    outFrameTextures.clear();
    outFrameTextures.reserve(graph.frames.size());
    for (auto &frame : graph.frames) {
      if (frame.outputIndex.has_value()) {
        size_t outputIndex = frame.outputIndex.value();
        if (outputIndex >= outputs.size())
          throw std::logic_error("Missing output");

        outFrameTextures.push_back(outputs[outputIndex]);
      } else if (frame.textureOverride) {
        outFrameTextures.push_back(frame.textureOverride);
      } else {
        TexturePtr texture =
            renderTextureCache.allocate(RenderTargetFormat{.format = frame.format, .size = frame.size}, frameCounter);
        outFrameTextures.push_back(texture);
      }
    }
  }

  void evaluate(const RenderGraph &graph, const std::vector<TexturePtr> &outputs, Context &context, size_t frameCounter) {
    // Evaluate all render steps in the order they are defined
    // potential optimizations:
    // - evaluate steps without dependencies in parralel
    // - group multiple steps into single render passes

    // Allocate frame textures
    getFrameTextures(frameTextures, outputs, graph, frameCounter);

    WGPUCommandEncoderDescriptor desc = {};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(context.wgpuDevice, &desc);

    for (const auto &node : graph.nodes) {
      WGPURenderPassDescriptor &renderPassDesc = createRenderPassDescriptor(graph, context, node);
      if (node.setupPass)
        node.setupPass(renderPassDesc);
      WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
      EvaluateNodeContext ctx{
          .encoder = renderPassEncoder,
          .evaluator = *this,
          .graph = graph,
          .node = node,
      };
      if (node.body)
        node.body(ctx);
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
