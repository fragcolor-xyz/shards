#ifndef CA95E36A_DF18_4EE1_B394_4094F976B20E
#define CA95E36A_DF18_4EE1_B394_4094F976B20E

#include "gfx/fwd.hpp"
#include "gfx_wgpu.hpp"
#include "linalg.h"
#include "texture.hpp"
#include "drawable.hpp"
#include "mesh.hpp"
#include "feature.hpp"
#include "params.hpp"
#include "render_target.hpp"
#include "shader/struct_layout.hpp"
#include "shader/textures.hpp"
#include "shader/fmt.hpp"
#include "renderer.hpp"
#include "log.hpp"
#include "graph.hpp"
#include "hasherxxh128.hpp"
#include "../core/pool.hpp"
#include "worker_memory.hpp"
#include <shards/core/pmr/wrapper.hpp>
#include <shards/core/pmr/unordered_map.hpp>
#include <shards/core/pmr/string.hpp>
#include <cassert>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>
#include <map>
#include <compare>
#include <optional>
#include <spdlog/spdlog.h>

namespace gfx::detail {

using shader::NumType;
using shader::StructLayout;
using shader::StructLayoutItem;
using shader::TextureBindingLayout;

inline auto getLogger() {
  static auto logger = gfx::getLogger();
  return logger;
}

// Wraps an object that is swapped per frame for double+ buffered rendering
template <typename TInner, size_t MaxSize_> struct Swappable {
private:
  std::optional<TInner> elems[MaxSize_];

public:
  template <typename... TArgs> Swappable(TArgs... args) {
    for (size_t i = 0; i < MaxSize_; i++) {
      elems[i].emplace(std::forward<TArgs>(args)...);
    }
  }

  template <typename T> void forAll(T &&callback) {
    for (size_t i = 0; i < MaxSize_; i++) {
      callback(elems[i].value());
    }
  }

  TInner &get(size_t frameNumber) {
    shassert(frameNumber <= MaxSize_);
    return elems[frameNumber].value();
  }

  TInner &operator()(size_t frameNumber) { return get(frameNumber); }
};

typedef uint16_t TextureId;

struct TextureBindings {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

  shards::pmr::vector<TextureContextData *> textures;

  TextureBindings() = default;
  TextureBindings(allocator_type allocator) : textures(allocator) {}

  template <typename H> void hash(H &hasher) const { hasher(textures.data(), textures.size() * sizeof(TextureId)); }
};

struct PipelineDrawables;

struct BufferBinding {
  FastString name;
  StructLayout layout;
  size_t index;
  shader::Dimension dimension;
  // External buffers are defined by features and are not part of the pipeline internals
  // Examples of internal buffers are the "object" and "view" buffer
  bool isExternal{};
};

struct RenderTargetLayout {
  struct Target {
    FastString name;
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

/// <div rustbindgen hide></div>
struct ParameterStorage final : public IParameterCollector {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

  shards::pmr::unordered_map<FastString, NumParameter> basic;
  shards::pmr::unordered_map<FastString, TextureParameter> textures;
  boost::container::flat_map<FastString, BufferPtr> blocks;

  using IParameterCollector::setParam;
  using IParameterCollector::setTexture;

  ParameterStorage() = default;
  ParameterStorage(allocator_type allocator) : basic(allocator), textures(allocator) {}
  ParameterStorage(ParameterStorage &&other, allocator_type allocator) : basic(allocator), textures(allocator) {
    *this = std::move(other);
  }
  ParameterStorage &operator=(ParameterStorage &&) = default;

  void setParam(FastString name, NumParameter &&value) { basic.insert_or_assign(name, std::move(value)); }
  void setTexture(FastString name, TextureParameter &&value) { textures.insert_or_assign(name, std::move(value)); }
  void setBlock(FastString name, BufferPtr &&value) { blocks.insert_or_assign(name, std::move(value)); }

  void setParamIfUnset(FastString key, const NumParameter &value) { basic.emplace(key, value); }

  void append(const ParameterStorage &other) {
    for (auto &it : other.basic) {
      basic.emplace(it);
    }
    for (auto &it : other.textures) {
      textures.emplace(it);
    }
    for (auto &it : other.blocks) {
      blocks.emplace(it);
    }
  }
};

struct CompilationError {
  std::string message;

  CompilationError() = default;
  CompilationError(std::string &&message) : message(std::move(message)) {}
  CompilationError(const std::string &message) : message(message) {}
};

template <typename CB> struct CachedFeatureGenerator {
  CB callback;
  std::weak_ptr<Feature> owningFeature;
  std::vector<std::weak_ptr<Feature>> otherFeatures;
};

struct BufferBindingRef {
  size_t bindGroup;
  size_t bufferIndex;
};

struct CachedPipeline {
  // The compiled shader module including both vertex/fragment entry points
  WgpuHandle<WGPUShaderModule> shaderModule;

  // The compiled pipeline layout
  WgpuHandle<WGPUPipelineLayout> pipelineLayout;

  // The compiled pipeline
  WgpuHandle<WGPURenderPipeline> pipeline;

  // All compiled bind group layouts used in the pipeline
  std::vector<WgpuHandle<WGPUBindGroupLayout>> bindGroupLayouts;

  // Ordered list of dynamic buffer offsets and in what order they should appear during rendering
  std::vector<BufferBindingRef> dynamicBufferRefs;

  // Supported texture bindings
  TextureBindingLayout textureBindingLayout;

  // The output format
  RenderTargetLayout renderTargetLayout;

  // Processor that this pipeline is built for
  IDrawableProcessor *drawableProcessor{};

  std::vector<BufferBinding> viewBuffersBindings;
  std::vector<BufferBinding> drawBufferBindings;

  // Collected parameter generator
  std::vector<CachedFeatureGenerator<FeatureGenerator::PerView>> perViewGenerators;
  std::vector<CachedFeatureGenerator<FeatureGenerator::PerObject>> perObjectGenerators;

  size_t lastTouched{};

  ParameterStorage baseViewParameters;
  ParameterStorage baseDrawParameters;

  std::optional<CompilationError> compilationError{};

  template <typename T> static const BufferBinding *findBufferBinding(const T &vec, FastString name) {
    auto it = std::find_if(vec.begin(), vec.end(), [&](const auto &binding) { return name == binding.name; });
    if (it == vec.end())
      return nullptr;
    return &*it;
  }

  const BufferBinding *findDrawBufferBinding(FastString name) const { return findBufferBinding(drawBufferBindings, name); }
  const BufferBinding *findViewBufferBinding(FastString name) const { return findBufferBinding(viewBuffersBindings, name); }

  const BufferBinding &resolveBufferBindingRef(BufferBindingRef ref) const;
};
typedef std::shared_ptr<CachedPipeline> CachedPipelinePtr;

struct CachedView {
  float4x4 projectionTransform;
  float4x4 invProjectionTransform;
  float4x4 invViewTransform;
  float4x4 previousViewTransform = linalg::identity;
  float4x4 currentViewTransform = linalg::identity;
  float4x4 viewProjectionTransform;
  bool isFlipped{};

  size_t lastTouched{};

  void touchWithNewTransform(const float4x4 &viewTransform, const float4x4 &projectionTransform, size_t frameCounter) {
    // Update history (but only at most once per frame)
    if (frameCounter > lastTouched) {
      previousViewTransform = currentViewTransform;
      currentViewTransform = viewTransform;
      lastTouched = frameCounter;
    }

    invViewTransform = linalg::inverse(viewTransform);

    isFlipped = isFlippedCoordinateSpace(viewTransform);

    this->projectionTransform = projectionTransform;
    invProjectionTransform = linalg::inverse(projectionTransform);

    viewProjectionTransform = linalg::mul(projectionTransform, currentViewTransform);
  }
};
typedef std::shared_ptr<CachedView> CachedViewDataPtr;

struct ViewData {
  ViewPtr view;
  CachedView &cachedView;
  std::optional<Rect> viewport;
  RenderTargetPtr renderTarget;
  int2 referenceOutputSize;
};

struct VertexStateBuilder {
  std::vector<WGPUVertexAttribute> attributes;
  WGPUVertexBufferLayout vertexLayout = {};

  void build(WGPUVertexState &vertex, const MeshFormat &meshFormat, WGPUShaderModule shaderModule) {
    size_t vertexStride = 0;
    size_t shaderLocationCounter = 0;
    for (auto &attr : meshFormat.vertexAttributes) {
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
    vertex.module = shaderModule;
  }
};

struct PooledWGPUBuffer {
  WgpuHandle<WGPUBuffer> buffer;
  size_t capacity;

  PooledWGPUBuffer() = default;
  PooledWGPUBuffer(PooledWGPUBuffer &&) = default;
  PooledWGPUBuffer &operator=(PooledWGPUBuffer &&) = default;

  operator WGPUBuffer() const { return buffer; }
};

struct PooledWGPUBufferTraits {
  PooledWGPUBuffer newItem() { return PooledWGPUBuffer(); }
  void release(PooledWGPUBuffer &) {}
  bool canRecycle(PooledWGPUBuffer &v) { return true; }
  void recycled(PooledWGPUBuffer &v) {}
};

inline constexpr auto findBufferInPoolBySize(size_t targetSize) {
  if constexpr (sizeof(size_t) >= 8) {
    if (targetSize > INT64_MAX) {
      throw std::runtime_error("targetSize too large");
    }
  }
  return [targetSize](PooledWGPUBuffer &buffer) -> int64_t {
    if (buffer.capacity < targetSize) {
      return INT64_MAX;
    }

    // Negate so the smallest buffer will be picked first
    return -(int64_t(buffer.capacity) - int64_t(targetSize));
  };
}

// Pool of WGPUBuffers with custom initializer
struct WGPUBufferPool : public shards::Pool<PooledWGPUBuffer, PooledWGPUBufferTraits> {
  using InitFunction = std::function<WGPUBuffer(size_t)>;
  InitFunction initFn;

  static WGPUBuffer defaultInitializer(size_t) { throw std::runtime_error("invalid buffer initializer"); }
  WGPUBufferPool(InitFunction &&init_ = &defaultInitializer) : initFn(std::move(init_)) {}

  PooledWGPUBuffer &allocateBuffer(size_t size) {
    return this->newValue(
        [&](PooledWGPUBuffer &buffer) {
          buffer.capacity = alignTo(size, 512);
          buffer.buffer.reset(initFn(buffer.capacity));
        },
        findBufferInPoolBySize(size));
  }
};

struct SharedPooledWGPUBufferTraits {
  using Ptr = std::shared_ptr<PooledWGPUBuffer>;
  Ptr newItem() { return std::make_shared<PooledWGPUBuffer>(); }
  void release(Ptr &) {}
  bool canRecycle(Ptr &v) { return v.use_count() == 1; }
  void recycled(Ptr &v) {}
};

inline constexpr auto findSharedBufferInPoolBySize(size_t targetSize) {
  if constexpr (sizeof(size_t) >= 8) {
    if (targetSize > INT64_MAX) {
      throw std::runtime_error("targetSize too large");
    }
  }
  return [targetSize](std::shared_ptr<PooledWGPUBuffer> &buffer) -> int64_t {
    if (buffer->capacity < targetSize) {
      return INT64_MAX;
    }

    // Negate so the smallest buffer will be picked first
    return -(int64_t(buffer->capacity) - int64_t(targetSize));
  };
}

struct WGPUSharedBufferPool : public shards::Pool<std::shared_ptr<PooledWGPUBuffer>, SharedPooledWGPUBufferTraits> {
  using Ptr = std::shared_ptr<PooledWGPUBuffer>;
  using InitFunction = std::function<WGPUBuffer(size_t)>;
  InitFunction initFn;

  static WGPUBuffer defaultInitializer(size_t) { throw std::runtime_error("invalid buffer initializer"); }
  WGPUSharedBufferPool(InitFunction &&init_ = &defaultInitializer) : initFn(std::move(init_)) {}

  Ptr &allocateBuffer(size_t size) {
    return this->newValue(
        [&](std::shared_ptr<PooledWGPUBuffer> &buffer) {
          buffer->capacity = alignTo(size, 1024);
          buffer->buffer.reset(initFn(buffer->capacity));
        },
        findSharedBufferInPoolBySize(size));
  }
};

// Data from generators
struct GeneratorData {
  ParameterStorage *viewParameters;
  shards::pmr::vector<ParameterStorage> *drawParameters;
};

} // namespace gfx::detail

#endif /* CA95E36A_DF18_4EE1_B394_4094F976B20E */
