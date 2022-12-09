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
#include "shader/fmt.hpp"
#include "dynamic_wgpu_buffer.hpp"
#include "renderer.hpp"
#include "log.hpp"
#include "graph.hpp"
#include "hasherxxh128.hpp"
#include <cassert>
#include <vector>
#include <map>
#include <compare>
#include <spdlog/spdlog.h>

namespace gfx::detail {
using shader::FieldType;
using shader::TextureBindingLayout;
using shader::UniformBufferLayout;
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

struct SortableDrawable;
struct DrawGroupKey {
  MeshContextData *meshData{};
  TextureIds textures{};
  std::optional<int4> clipRect{};

  DrawGroupKey() = default;
  DrawGroupKey(const IDrawable &drawable, const TextureIds &textureIds) {}
  // TODO: Processor
      // : meshData(drawable.mesh->contextData.get()), textures(textureIds), clipRect(drawable.clipRect) {}

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
  const IDrawable *drawable{};
  const CachedDrawable *cachedDrawable{};
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

struct BufferBinding {
  UniformBufferLayout layout;
  size_t index;
};

// Keeps track of
struct InstanceBufferDesc {
  size_t bufferLength;
  size_t bufferStride;
};

struct RenderTargetLayout {
  struct Target {
    std::string name;
    WGPUTextureFormat format;

    std::strong_ordering operator<=>(const Target &other) const = default;

    template <typename T> void getPipelineHash(T &hasher) const {
      hasher(name);
      hasher(format);
    }
  };

  std::vector<Target> targets;
  std::optional<size_t> depthTargetIndex;

  bool operator==(const RenderTargetLayout &other) const {
    if (!std::equal(targets.begin(), targets.end(), other.targets.begin(), other.targets.end()))
      return false;

    if (depthTargetIndex != other.depthTargetIndex)
      return false;

    return true;
  }

  bool operator!=(const RenderTargetLayout &other) const { return !(*this == other); }

  template <typename T> void getPipelineHash(T &hasher) const {
    hasher(targets);
    hasher(depthTargetIndex);
  }
};

struct CachedPipeline {
  MeshFormat meshFormat;
  std::vector<const Feature *> features;
  std::map<std::string, TextureParameter> materialTextureBindings;
  TextureBindingLayout textureBindingLayout;
  RenderTargetLayout renderTargetLayout;

  WgpuHandle<WGPURenderPipeline> pipeline;
  WgpuHandle<WGPUShaderModule> shaderModule;
  WgpuHandle<WGPUPipelineLayout> pipelineLayout;
  std::vector<WgpuHandle<WGPUBindGroupLayout>> bindGroupLayouts;

  std::vector<BufferBinding> viewBuffersBindings;
  std::vector<BufferBinding> drawBufferBindings;
  std::vector<InstanceBufferDesc> drawBufferInstanceDescriptions;

  std::vector<WgpuHandle<WGPUBuffer>> viewBuffers;
  WgpuHandle<WGPUBindGroup> viewBindGroup;

  std::vector<WGPUBindGroupEntry> drawBindGroupEntries;

  // Pool to allocate instance buffers from
  DynamicWGPUBufferPool instanceBufferPool;

  ParameterStorage baseDrawData;

  size_t lastTouched{};

  void resetPools() { instanceBufferPool.reset(); }
};
typedef std::shared_ptr<CachedPipeline> CachedPipelinePtr;

struct PipelineDrawables {
  CachedPipelinePtr cachedPipeline;

  std::vector<const IDrawable *> drawables;
  std::vector<SortableDrawable> drawablesSorted;
  std::vector<DrawGroup> drawGroups;
  TextureIdMap textureIdMap;
};

inline void packDrawData(uint8_t *outData, size_t outSize, const UniformBufferLayout &layout, const ParameterStorage &parameterStorage) {
  size_t layoutIndex = 0;
  for (auto &fieldName : layout.fieldNames) {
    auto drawDataIt = parameterStorage.data.find(fieldName);
    if (drawDataIt != parameterStorage.data.end()) {
      const UniformLayout &itemLayout = layout.items[layoutIndex];
      FieldType drawDataFieldType = getParamVariantType(drawDataIt->second);
      if (itemLayout.type != drawDataFieldType) {
        SPDLOG_LOGGER_WARN(getLogger(), "WEBGPU: Field type mismatch layout:{} parameterStorage:{}", itemLayout.type, drawDataFieldType);
        continue;
      }

      packParamVariant(outData + itemLayout.offset, outSize - itemLayout.offset, drawDataIt->second);
    }
    layoutIndex++;
  }
}

struct CachedView {
  DynamicWGPUBufferPool viewBuffers;
  float4x4 projectionTransform;
  float4x4 invProjectionTransform;
  float4x4 invViewTransform;
  float4x4 previousViewTransform = linalg::identity;
  float4x4 currentViewTransform = linalg::identity;
  float4x4 viewProjectionTransform;

  size_t lastTouched{};

  void touchWithNewTransform(const float4x4 &viewTransform, const float4x4 &projectionTransform, size_t frameCounter) {
    if (frameCounter > lastTouched) {
      previousViewTransform = currentViewTransform;
      currentViewTransform = viewTransform;
      invViewTransform = linalg::inverse(viewTransform);

      this->projectionTransform = projectionTransform;
      invProjectionTransform = linalg::inverse(projectionTransform);

      viewProjectionTransform = linalg::mul(projectionTransform, currentViewTransform);

      lastTouched = frameCounter;
    }
  }

  void resetPools() { viewBuffers.reset(); }
};
typedef std::shared_ptr<CachedView> CachedViewDataPtr;

struct CachedDrawable {
  float4x4 previousTransform = linalg::identity;
  float4x4 currentTransform = linalg::identity;

  size_t lastTouched{};

  void touchWithNewTransform(const float4x4 &transform, size_t frameCounter) {
    if (frameCounter > lastTouched) {
      previousTransform = currentTransform;
      currentTransform = transform;

      lastTouched = frameCounter;
    }
  }
};

typedef std::shared_ptr<CachedDrawable> CachedDrawablePtr;

struct FrameReferences {
  std::vector<std::shared_ptr<ContextData>> contextDataReferences;
  void clear() { contextDataReferences.clear(); }
};

struct ViewData {
  View &view;
  CachedView &cachedView;
  Rect viewport;
  WGPUBuffer viewBuffer;
  RenderTargetPtr renderTarget;
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
      size_t typeSize = getStorageTypeSize(attr.type);
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
