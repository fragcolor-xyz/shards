#ifndef CA95E36A_DF18_4EE1_B394_4094F976B20E
#define CA95E36A_DF18_4EE1_B394_4094F976B20E

#include "gfx_wgpu.hpp"
#include "texture.hpp"
#include "drawable.hpp"
#include "mesh.hpp"
#include "feature.hpp"
#include "params.hpp"
#include "render_target.hpp"
#include "shader/uniforms.hpp"
#include "shader/textures.hpp"
#include "dynamic_wgpu_buffer.hpp"
#include "renderer.hpp"
#include "log.hpp"
#include "graph.hpp"
#include "hasherxxh128.hpp"
#include <cassert>
#include <vector>
#include <map>
#include <spdlog/spdlog.h>

namespace gfx::detail {
using shader::FieldType;
using shader::FieldTypes;
using shader::TextureBindingLayout;
using shader::TextureBindingLayoutBuilder;
using shader::UniformBufferLayout;
using shader::UniformBufferLayoutBuilder;
using shader::UniformLayout;

inline auto getLogger() {
  static auto logger = gfx::getLogger();
  return logger;
}

// Wraps an object that is swapped per frame for double+ buffered rendering
template <typename TInner, size_t MaxSize> struct Swappable {
  TInner elems[MaxSize];
  TInner &get(size_t frameNumber) {
    assert(frameNumber <= MaxSize);
    return elems[frameNumber];
  }
  TInner &operator()(size_t frameNumber) { return get(frameNumber); }
};

typedef uint16_t TextureId;

struct TextureIds {
  std::vector<TextureId> textures;

  bool operator==(const TextureIds &other) const {
    return std::equal(textures.begin(), textures.end(), other.textures.begin(), other.textures.end());
  }

  bool operator!=(const TextureIds &other) const { return !(*this == other); }

  bool operator<(const TextureIds &other) const {
    if (textures.size() != other.textures.size())
      return textures.size() < other.textures.size();
    for (size_t i = 0; i < textures.size(); i++) {
      if (textures[i] != other.textures[i]) {
        return textures[i] < other.textures[i];
      }
    }
    return false;
  }
};

struct TextureIdMap {
private:
  std::map<Texture *, TextureId> mapping;
  std::vector<std::shared_ptr<TextureContextData>> textureData;

public:
  TextureId assign(Texture *texture) {
    if (!texture)
      return ~0;

    auto it = mapping.find(texture);
    if (it != mapping.end()) {
      return it->second;
    } else {
      size_t newId = textureData.size();
      assert(newId <= UINT16_MAX);

      TextureId id = TextureId(newId);
      textureData.emplace_back(texture->contextData);
      mapping.insert_or_assign(texture, id);
      return id;
    }
  }

  TextureContextData *resolve(TextureId id) const {
    if (id == TextureId(~0))
      return nullptr;
    else {
      return textureData[id].get();
    }
  }

  // calls callback(Texture* texture)
  template <typename TCallback> void forEachTexture(TCallback callback) const {
    for (auto &pair : mapping)
      callback(pair.first);
  }

  void reset() {
    mapping.clear();
    textureData.clear();
  }
};

struct RenderTargetLayout {
  struct DepthTarget {
    WGPUTextureFormat format;

    bool operator==(const DepthTarget &other) const { return format == other.format; }
    bool operator!=(const DepthTarget &other) const { return !(*this == other); }

    template <typename T> void hashStatic(T &hasher) const {
      hasher("<depth>");
      hasher(format);
    }
  };

  struct ColorTarget {
    std::string name;
    WGPUTextureFormat format;

    bool operator==(const ColorTarget &other) const { return name == other.name && format == other.format; }
    bool operator!=(const ColorTarget &other) const { return !(*this == other); }

    template <typename T> void hashStatic(T &hasher) const {
      hasher(name);
      hasher(format);
    }
  };

  std::optional<DepthTarget> depthTarget;
  std::vector<ColorTarget> colorTargets;

  bool operator==(const RenderTargetLayout &other) const {
    if (!std::equal(colorTargets.begin(), colorTargets.end(), other.colorTargets.begin(), other.colorTargets.end()))
      return false;

    if (depthTarget != other.depthTarget)
      return false;

    return true;
  }

  bool operator!=(const RenderTargetLayout &other) const { return !(*this == other); }

  template <typename T> void hashStatic(T &hasher) const {
    hasher(depthTarget);
    hasher(colorTargets);
  }
};

struct SortableDrawable;
struct DrawGroupKey {
  MeshContextData *meshData{};
  TextureIds textures{};
  std::optional<int4> clipRect{};

  DrawGroupKey() = default;
  DrawGroupKey(const Drawable &drawable, const TextureIds &textureIds)
      : meshData(drawable.mesh->contextData.get()), textures(textureIds), clipRect(drawable.clipRect) {}

  bool operator==(const DrawGroupKey &other) const {
    return meshData == other.meshData && textures == other.textures && clipRect == other.clipRect;
  }
  bool operator!=(const DrawGroupKey &other) const { return !(*this == other); }

  bool operator<(const DrawGroupKey &other) const {
    if (meshData != other.meshData) {
      return size_t(meshData) < size_t(other.meshData);
    }
    if (clipRect != other.clipRect) {
      return clipRect < other.clipRect;
    }
    return textures < other.textures;
  }
};

struct SortableDrawable {
  const Drawable *drawable{};
  TextureIds textureIds;
  DrawGroupKey key;
  float projectedDepth = 0.0f;
};

struct DrawGroup {
  DrawGroupKey key;
  size_t startIndex;
  size_t numInstances;
  DrawGroup(const DrawGroupKey &key, size_t startIndex, size_t numInstances)
      : key(key), startIndex(startIndex), numInstances(numInstances) {}
};

struct CachedPipeline {
  MeshFormat meshFormat;
  std::vector<const Feature *> features;
  UniformBufferLayout objectBufferLayout;
  std::map<std::string, TextureParameter> materialTextureBindings;
  TextureBindingLayout textureBindingLayout;
  RenderTargetLayout renderTargetLayout;
  DrawData baseDrawData;

  WgpuHandle<WGPURenderPipeline> pipeline;
  WgpuHandle<WGPUShaderModule> shaderModule;
  WgpuHandle<WGPUPipelineLayout> pipelineLayout;
  std::vector<WgpuHandle<WGPUBindGroupLayout>> bindGroupLayouts;

  // Pool to allocate instance buffers from
  DynamicWGPUBufferPool instanceBufferPool;

  void resetPools() { instanceBufferPool.reset(); }
};
typedef std::shared_ptr<CachedPipeline> CachedPipelinePtr;

struct PipelineDrawables {
  CachedPipelinePtr cachedPipeline;

  std::vector<const Drawable *> drawables;
  std::vector<SortableDrawable> drawablesSorted;
  std::vector<DrawGroup> drawGroups;
  TextureIdMap textureIdMap;
};

inline void packDrawData(uint8_t *outData, size_t outSize, const UniformBufferLayout &layout, const DrawData &drawData) {
  size_t layoutIndex = 0;
  for (auto &fieldName : layout.fieldNames) {
    auto drawDataIt = drawData.data.find(fieldName);
    if (drawDataIt != drawData.data.end()) {
      const UniformLayout &itemLayout = layout.items[layoutIndex];
      FieldType drawDataFieldType = getParamVariantType(drawDataIt->second);
      if (itemLayout.type != drawDataFieldType) {
        SPDLOG_LOGGER_WARN(getLogger(), "WEBGPU: Field type mismatch layout:{} drawData:{}", itemLayout.type, drawDataFieldType);
        continue;
      }

      packParamVariant(outData + itemLayout.offset, outSize - itemLayout.offset, drawDataIt->second);
    }
    layoutIndex++;
  }
}

struct CachedViewData {
  DynamicWGPUBufferPool viewBuffers;
  float4x4 projectionMatrix;
  ~CachedViewData() {}

  void resetPools() { viewBuffers.reset(); }
};
typedef std::shared_ptr<CachedViewData> CachedViewDataPtr;

struct FrameReferences {
  std::vector<std::shared_ptr<ContextData>> contextDataReferences;
  void clear() { contextDataReferences.clear(); }
};

struct Bindable {
  WGPUBuffer buffer;
  UniformBufferLayout layout;
  size_t overrideSize = ~0;
  size_t offset = 0;
  Bindable(WGPUBuffer buffer, UniformBufferLayout layout, size_t overrideSize = ~0, size_t offset = 0)
      : buffer(buffer), layout(layout), overrideSize(overrideSize), offset(offset) {}

  WGPUBindGroupEntry toBindGroupEntry(size_t &bindingCounter) const {
    WGPUBindGroupEntry entry{};
    entry.binding = bindingCounter++;
    entry.buffer = buffer;
    entry.size = (overrideSize != size_t(~0)) ? overrideSize : layout.size;
    entry.offset = offset;
    return entry;
  }
};

struct RenderTargetData {
  struct DepthTarget {
    Texture *texture;

    bool operator==(const DepthTarget &other) const { return texture == other.texture; }
  };

  struct ColorTarget {
    std::string name;
    Texture *texture;

    bool operator==(const ColorTarget &other) const { return name == other.name && texture == other.texture; }
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
        .name = "main",
        .texture = mainOutput.texture.get(),
    });
  }

  // Resolve layout info without the actual bindings
  RenderTargetLayout getLayout() const {
    RenderTargetLayout result;
    if (depthTarget) {
      result.depthTarget = RenderTargetLayout::DepthTarget{
          .format = depthTarget->texture->getFormat().pixelFormat,
      };
    }

    for (auto &colorTarget : colorTargets) {
      result.colorTargets.push_back(RenderTargetLayout::ColorTarget{
          .name = colorTarget.name,
          .format = colorTarget.texture->getFormat().pixelFormat,
      });
    }

    return result;
  }

  bool operator==(const RenderTargetData &other) const {
    if (!std::equal(colorTargets.begin(), colorTargets.end(), other.colorTargets.begin(), other.colorTargets.end()))
      return false;

    if (depthTarget != other.depthTarget)
      return false;

    return true;
  }
};

struct RenderGraphNode {
  RenderTargetData renderTargetData;
  std::vector<Texture *> writesTo;
  std::vector<Texture *> readsFrom;

  std::function<void(WGPURenderPassDescriptor &)> setupPass;
  std::function<void(WGPURenderPassEncoder)> body;

  void populateWritesTo() {
    if (renderTargetData.depthTarget)
      writesTo.push_back(renderTargetData.depthTarget->texture);
    for (auto &target : renderTargetData.colorTargets) {
      writesTo.push_back(target.texture);
    }
  }

  RenderGraphNode() = default;
  RenderGraphNode(RenderGraphNode &&) = default;
  RenderGraphNode &operator=(RenderGraphNode &&) = default;
};

struct RenderGraph {
  std::vector<RenderGraphNode> nodes;
  std::vector<Texture *> writesTo;
  std::vector<Texture *> readFrom;

  void populateEdges() {
    std::map<Texture *, int32_t> writtenBy;
    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++) {
      auto &node = nodes[nodeIndex];
      auto &rtd = node.renderTargetData;
      if (rtd.depthTarget)
        node.writesTo.push_back(rtd.depthTarget->texture);
      for (auto &target : rtd.colorTargets) {
        node.writesTo.push_back(target.texture);
      }
    }
  }
};

struct ViewData {
  View &view;
  Rect viewport;
  WGPUBuffer viewBuffer;
  RenderTargetPtr renderTarget;
};

struct RenderGraphEvaluator {
private:
  std::set<Texture *> writtenTextures;
  std::vector<WGPURenderPassColorAttachment> cachedColorAttachments;
  std::optional<WGPURenderPassDepthStencilAttachment> cachedDepthStencilAttachment;
  WGPURenderPassDescriptor cachedRenderPassDescriptor{};

public:
  bool isWrittenTo(const TexturePtr &texture) const { return writtenTextures.contains(texture.get()); }

  void clearColorTexture(Context &context, const TexturePtr &texture, WGPUCommandEncoder commandEncoder) {
    RenderTargetData rtd{
        .colorTargets = {RenderTargetData::ColorTarget{
            .name = "main",
            .texture = texture.get(),
        }},
    };

    auto &passDesc = createRenderPassDescriptor(context, rtd);
    auto &attachment = const_cast<WGPURenderPassColorAttachment &>(passDesc.colorAttachments[0]);
    attachment.loadOp = WGPULoadOp_Clear;

    WGPURenderPassEncoder clearPass = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);
    wgpuRenderPassEncoderEnd(clearPass);
  }

  // Describes a render pass so that targets are cleared to their default value or loaded
  // based on if they were already written to previously
  WGPURenderPassDescriptor &createRenderPassDescriptor(Context &context, const RenderTargetData &renderTargetData) {
    cachedColorAttachments.clear();
    cachedDepthStencilAttachment.reset();

    memset(&cachedRenderPassDescriptor, 0, sizeof(cachedRenderPassDescriptor));

    for (auto target : renderTargetData.colorTargets) {
      auto &textureData = target.texture->createContextDataConditional(context);
      auto &attachment = cachedColorAttachments.emplace_back(WGPURenderPassColorAttachment{
          .view = textureData.defaultView,
          .clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0},
      });

      bool previouslyWrittenTo = writtenTextures.contains(target.texture);
      if (previouslyWrittenTo) {
        attachment.loadOp = WGPULoadOp_Load;
      } else {
        attachment.loadOp = WGPULoadOp_Clear;
      }
      attachment.storeOp = WGPUStoreOp_Store;

      writtenTextures.insert(target.texture);
    }

    if (renderTargetData.depthTarget) {
      auto &target = renderTargetData.depthTarget.value();
      auto &textureData = target.texture->createContextDataConditional(context);
      cachedDepthStencilAttachment = WGPURenderPassDepthStencilAttachment{
          .view = textureData.defaultView,
          .depthClearValue = 1.0f,
          .stencilClearValue = 0,
      };
      auto &attachment = cachedDepthStencilAttachment.value();

      bool previouslyWrittenTo = writtenTextures.contains(target.texture);
      if (previouslyWrittenTo) {
        attachment.depthLoadOp = WGPULoadOp_Load;
      } else {
        attachment.depthLoadOp = WGPULoadOp_Clear;
      }
      attachment.depthStoreOp = WGPUStoreOp_Store;
      attachment.stencilLoadOp = WGPULoadOp_Undefined;
      attachment.stencilStoreOp = WGPUStoreOp_Undefined;

      writtenTextures.insert(target.texture);
    }

    cachedRenderPassDescriptor.colorAttachments = cachedColorAttachments.data();
    cachedRenderPassDescriptor.colorAttachmentCount = cachedColorAttachments.size();
    cachedRenderPassDescriptor.depthStencilAttachment =
        cachedDepthStencilAttachment ? &cachedDepthStencilAttachment.value() : nullptr;

    return cachedRenderPassDescriptor;
  }

  // Reset all render texture write state
  void reset() { writtenTextures.clear(); }

  void evaluate(const RenderGraph &graph, Context &context) {
    // Evaluate all render steps in the order they are defined
    // potential optimizations:
    // - evaluate steps without dependencies in parralel
    // - group multiple steps into single render passes

    WGPUCommandEncoderDescriptor desc = {};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(context.wgpuDevice, &desc);

    for (const auto &node : graph.nodes) {
      WGPURenderPassDescriptor &renderPassDesc = createRenderPassDescriptor(context, node.renderTargetData);
      if (node.setupPass)
        node.setupPass(renderPassDesc);
      WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
      node.body(renderPassEncoder);
      wgpuRenderPassEncoderEnd(renderPassEncoder);
    }

    WGPUCommandBufferDescriptor cmdBufDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

    context.submit(cmdBuf);
  }
};

struct VertexStateBuilder {
  std::vector<WGPUVertexAttribute> attributes;
  WGPUVertexBufferLayout vertexLayout = {};

  void build(WGPUVertexState &vertex, CachedPipeline &cachedPipeline) {
    size_t vertexStride = 0;
    size_t shaderLocationCounter = 0;
    for (auto &attr : cachedPipeline.meshFormat.vertexAttributes) {
      WGPUVertexAttribute &wgpuAttribute = attributes.emplace_back();
      wgpuAttribute.offset = uint64_t(vertexStride);
      wgpuAttribute.format = getWGPUVertexFormat(attr.type, attr.numComponents);
      wgpuAttribute.shaderLocation = shaderLocationCounter++;
      size_t typeSize = getVertexAttributeTypeSize(attr.type);
      vertexStride += attr.numComponents * typeSize;
    }
    vertexLayout.arrayStride = vertexStride;
    vertexLayout.attributeCount = attributes.size();
    vertexLayout.attributes = attributes.data();
    vertexLayout.stepMode = WGPUVertexStepMode::WGPUVertexStepMode_Vertex;

    vertex.bufferCount = 1;
    vertex.buffers = &vertexLayout;
    vertex.constantCount = 0;
    vertex.entryPoint = "vertex_main";
    vertex.module = cachedPipeline.shaderModule;
  }
};

} // namespace gfx::detail

#endif /* CA95E36A_DF18_4EE1_B394_4094F976B20E */
