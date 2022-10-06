#ifndef FBBE103B_4B4A_462F_BE67_FE59A0C30FB5
#define FBBE103B_4B4A_462F_BE67_FE59A0C30FB5

#include "renderer_types.hpp"

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
  RenderTargetLayout renderTargetLayout;
  std::vector<FrameIndex> writesTo;
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

    // Frame(const RenderStepIO::OutputVariant &output, int2 size) : output(output), size(size) {}

    // const std::string &getName() const {
    //   const std::string *namePtr{};
    //   std::visit([&](auto &arg) { namePtr = &arg.name; }, output);
    //   assert(namePtr);
    //   return *namePtr;
    // }

    // WGPUTextureFormat getFormat() const {
    //   WGPUTextureFormat format{};
    //   std::visit(
    //       [&](auto &arg) {
    //         using T = std::decay_t<decltype(arg)>;
    //         if constexpr (std::is_same_v<T, RenderStepIO::NamedOutput>) {
    //           format = arg.format;
    //         } else if constexpr (std::is_same_v<T, RenderStepIO::TextureOutput>) {
    //           TexturePtr handle = arg.handle;
    //           format = handle->getFormat().pixelFormat;
    //         }
    //       },
    //       output);
    //   return format;
    // }
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

  FrameIndex assignFrame(const RenderStepIO::OutputVariant &output, int2 targetSize) {
    FrameIndex frameIndex = ~0;
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;

          if constexpr (std::is_same_v<T, RenderStepIO::TextureOutput>) {
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
            if constexpr (std::is_same_v<T, RenderStepIO::TextureOutput>) {
              assert(arg.handle);
              frame.textureOverride = arg.handle;
              frame.format = frame.textureOverride->getFormat().pixelFormat;
            } else if constexpr (std::is_same_v<T, RenderStepIO::NamedOutput>) {
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

  void attachOutput(const std::string &name, TexturePtr texture) {
    WGPUTextureFormat outputTextureFormat = texture->getFormat().pixelFormat;

    // Go in reverse, and replace the last write to the given name to this texture
    for (size_t ir = 0; ir < nodes.size(); ir++) {
      size_t i = nodes.size() - 1 - ir;
      auto &node = nodes[i];
      for (auto &frameIndex : node.writesTo) {
        auto &frame = frames[frameIndex];
        if (frame.name == name) {
          FrameIndex outputFrameIndex = frames.size();
          size_t outputIndex = outputs.size();
          outputs.emplace_back(RenderGraph::Output{
              .name = name,
              .frameIndex = outputFrameIndex,
          });

          FrameIndex prevFrameIndex = frameIndex;
          frames.emplace_back(RenderGraph::Frame{
              .name = name,
              .size = frame.size,
              .format = outputTextureFormat,
              .outputIndex = outputIndex,
          });

          // Update output frame index
          frameIndex = outputFrameIndex;

          // Make sure no node after this references this as an input anymore
          // TODO: support this case by forcing a copy from intermediate texture to output
          checkNoReadFrom(i + 1, prevFrameIndex);

          return;
        }
      }
    }
  }

  // Call before allocating a node's outputs
  void allocateInputs(size_t nodeIndex, const RenderStepIO &io) {
    nodes.resize(std::max(nodes.size(), nodeIndex + 1));
    RenderGraphNode &node = nodes[nodeIndex];

    for (auto &inputName : io.inputs) {
      FrameIndex frameIndex{};
      if (findFrameIndex(inputName, frameIndex)) {
        node.readsFrom.push_back(frameIndex);
      }
    }
  }

  void allocateOutputs(size_t nodeIndex, const RenderStepIO &io) {
    nodes.resize(std::max(nodes.size(), nodeIndex + 1));
    RenderGraphNode &node = nodes[nodeIndex];

    for (auto &output : io.outputs) {
      int2 targetSize = int2(linalg::floor(referenceOutputSize * io.outputSizeScale.value_or(float2(1.0f))));

      FrameIndex frameIndex = assignFrame(output, targetSize);

      auto targetFormat = std::visit(
          [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, RenderStepIO::NamedOutput>) {
              return arg.format;
            } else if constexpr (std::is_same_v<T, RenderStepIO::TextureOutput>) {
              return arg.handle->getFormat().pixelFormat;
            }
            throw std::logic_error("");
          },
          output);

      bool sizeMismatch = frames[frameIndex].size != targetSize;
      bool formatMismatch = frames[frameIndex].format != targetFormat;

      // Check if the same frame is being read from
      auto it = std::find(node.readsFrom.begin(), node.readsFrom.end(), frameIndex);
      if (sizeMismatch || formatMismatch || it != node.readsFrom.end()) {
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

      node.writesTo.push_back(frameIndex);
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

  RenderGraph finalize() {
    RenderGraph graph{
        .nodes = std::move(nodes),
        .frames = std::move(frames),
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

  // Enqueue a clear command on the given color texture
  void clearColorTexture(Context &context, const TexturePtr &texture, WGPUCommandEncoder commandEncoder) {
    RenderTargetData rtd{
        .colorTargets = {RenderTargetData::ColorTarget{
            .name = "color",
            .texture = texture.get(),
        }},
    };

    // TODO
    /* auto &passDesc = createRenderPassDescriptor(context, rtd);
    auto &attachment = const_cast<WGPURenderPassColorAttachment &>(passDesc.colorAttachments[0]);
    attachment.loadOp = WGPULoadOp_Clear;

    WGPURenderPassEncoder clearPass = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);
    wgpuRenderPassEncoderEnd(clearPass); */
  }

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
      size_t frameIndex = node.writesTo[i];
      TexturePtr &texture = frameTextures[frameIndex];

      auto &textureData = texture->createContextDataConditional(context);
      bool previouslyWrittenTo = writtenTextures.contains(texture.get());

      if (i == depthIndex) {
        auto &textureData = texture->createContextDataConditional(context);
        cachedDepthStencilAttachment = WGPURenderPassDepthStencilAttachment{
            .view = textureData.defaultView,
            .depthClearValue = 1.0f,
            .stencilClearValue = 0,
        };
        auto &attachment = cachedDepthStencilAttachment.value();

        if (previouslyWrittenTo) {
          attachment.depthLoadOp = WGPULoadOp_Load;
        } else {
          attachment.depthLoadOp = WGPULoadOp_Clear;
        }
        attachment.depthStoreOp = WGPUStoreOp_Store;
        attachment.stencilLoadOp = WGPULoadOp_Undefined;
        attachment.stencilStoreOp = WGPUStoreOp_Undefined;
      } else {
        auto &attachment = cachedColorAttachments.emplace_back(WGPURenderPassColorAttachment{
            .view = textureData.defaultView,
            .clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0},
        });

        if (previouslyWrittenTo) {
          attachment.loadOp = WGPULoadOp_Load;
        } else {
          attachment.loadOp = WGPULoadOp_Clear;
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
