#ifndef GFX_FEATURE
#define GFX_FEATURE

#include "enums.hpp"
#include "fwd.hpp"
#include "linalg.hpp"
#include "params.hpp"
#include "shader/types.hpp"
#include "shader/entry_point.hpp"
#include "pipeline_hash_collector.hpp"
#include "unique_id.hpp"
#include "feature_generator.hpp"
#include <boost/container/flat_map.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

namespace gfx {
namespace detail {
struct CachedView;
} // namespace detail

namespace shader {
struct EntryPoint;
}

struct BlendComponent {
  // NOTE: Sync layout with WGPUBlendComponent
  WGPUBlendOperation operation{};
  WGPUBlendFactor srcFactor{};
  WGPUBlendFactor dstFactor{};

  template <typename T> void getPipelineHash(T &hasher) const {
    hasher(srcFactor);
    hasher(dstFactor);
    hasher(operation);
  }

  bool operator==(const BlendComponent &other) const {
    return operation == other.operation && srcFactor == other.srcFactor && dstFactor == other.dstFactor;
  }
  bool operator!=(const BlendComponent &other) const { return !(*this == other); }

  static BlendComponent Opaque;
  static BlendComponent Additive;
  static BlendComponent Alpha;
  static BlendComponent AlphaPremultiplied;
};

struct BlendState {
  // NOTE: Sync layout with WGPUBlendState
  BlendComponent color;
  BlendComponent alpha;

  operator const WGPUBlendState &() const { return *reinterpret_cast<const WGPUBlendState *>(this); }
  template <typename T> void getPipelineHash(T &hasher) const {
    hasher(color);
    hasher(alpha);
  }

  bool operator==(const BlendState &other) const { return color == other.color && alpha == other.alpha; }
  bool operator!=(const BlendState &other) const { return !(*this == other); }
};
} // namespace gfx

namespace std {
template <> struct hash<gfx::BlendComponent> {
  size_t operator()(const gfx::BlendComponent &v) {
    size_t result{};
    result = std::hash<WGPUBlendOperation>{}(v.operation);
    result = (result * 3) ^ std::hash<WGPUBlendFactor>{}(v.srcFactor);
    result = (result * 3) ^ std::hash<WGPUBlendFactor>{}(v.dstFactor);
    return result;
  }
};

template <> struct hash<gfx::BlendState> {
  size_t operator()(const gfx::BlendState &v) {
    size_t result{};
    result = std::hash<gfx::BlendComponent>{}(v.color);
    result = (result * 3) ^ std::hash<gfx::BlendComponent>{}(v.alpha);
    return result;
  }
};
} // namespace std

namespace gfx {

struct FeaturePipelineState {
#define PIPELINE_STATE(_name, _type)                            \
public:                                                         \
  std::optional<_type> _name;                                   \
                                                                \
public:                                                         \
  void set_##_name(const _type &_name) { this->_name = _name; } \
  void clear_##_name() { this->_name.reset(); }
#include "pipeline_states.def"

public:
  FeaturePipelineState combine(const FeaturePipelineState &other) const {
    FeaturePipelineState result = *this;
#define PIPELINE_STATE(_name, _type) \
  if (other._name)                   \
    result._name = other._name;
#include "pipeline_states.def"
    return result;
  }

  bool operator==(const FeaturePipelineState &other) const {
#define PIPELINE_STATE(_name, _type) \
  if (_name != other._name)          \
    return false;
#include "pipeline_states.def"
    return true;
  }

  bool operator!=(const FeaturePipelineState &other) const { return !(*this == other); }

  size_t getHash() const {
    size_t hash = 0;
#define PIPELINE_STATE(_name, _type) hash = (hash * 3) ^ std::hash<decltype(_name)>{}(_name);
#include "pipeline_states.def"
    return hash;
  }

  template <typename T> void getPipelineHash(T &hasher) const {
#define PIPELINE_STATE(_name, _type) hasher(_name);
#include "pipeline_states.def"
  }
};

struct RequiredAttributes {
  // When enabled, each vertex will be guaranteed to have a local basis encoded as a quaternion
  bool requirePerVertexLocalBasis{};
};

inline RequiredAttributes operator|(const RequiredAttributes &a, const RequiredAttributes &other) {
  return RequiredAttributes{
      .requirePerVertexLocalBasis = a.requirePerVertexLocalBasis || other.requirePerVertexLocalBasis,
  };
}

extern UniqueIdGenerator featureIdGenerator;
struct Feature : public std::enable_shared_from_this<Feature> {
  // Used to identify this feature for caching purposes
  const UniqueId id = featureIdGenerator.getNext();

  // Pipeline state flags
  FeaturePipelineState state;
  // Generated parameters and precomputed rendering
  std::vector<FeatureGenerator> generators;
  // Shader parameters read from per-instance buffer
  boost::container::flat_map<FastString, NumParamDecl> shaderParams;
  // Texture parameters
  boost::container::flat_map<FastString, TextureParamDecl> textureParams;
  // Params that are bound as a single binding
  boost::container::flat_map<FastString, BlockParamDecl> blockParams;
  // Shader entry points
  std::vector<shader::EntryPoint> shaderEntryPoints;

  PipelineModifierPtr pipelineModifier;

  RequiredAttributes requiredAttributes;

  virtual ~Feature() = default;

  UniqueId getId() const { return id; }

  void pipelineHashCollect(detail::PipelineHashCollector &) const;
};
typedef std::shared_ptr<Feature> FeaturePtr;

} // namespace gfx

#endif // GFX_FEATURE
