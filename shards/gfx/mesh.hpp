#ifndef A9FF2947_97BD_4662_A743_E2DB36A32FA2
#define A9FF2947_97BD_4662_A743_E2DB36A32FA2

#include "context_data.hpp"
#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "wgpu_handle.hpp"
#include "linalg/linalg.h"
#include "fwd.hpp"
#include "unique_id.hpp"
#include "log.hpp"
#include <string>
#include <vector>
#include <optional>

namespace gfx {

namespace detail {
struct PipelineHashCollector;
}

/// <div rustbindgen opaque></div>
struct MeshVertexAttribute {
  FastString name;
  uint8_t numComponents;
  StorageType type;

  MeshVertexAttribute() = default;
  MeshVertexAttribute(FastString name, uint8_t numComponents, StorageType type = StorageType::Float32)
      : name(name), numComponents(numComponents), type(type) {}

  // Compares everthing except for the name
  bool isSameDataFormat(const MeshVertexAttribute &other) const {
    return numComponents == other.numComponents && type == other.type;
  }

  template <typename T> void getPipelineHash(T &hasher) const {
    hasher(name);
    hasher(numComponents);
    hasher(type);
  }
};

/// <div rustbindgen opaque></div>
struct MeshFormat {
  PrimitiveType primitiveType = PrimitiveType::TriangleList;
  WindingOrder windingOrder = WindingOrder::CCW;
  IndexFormat indexFormat = IndexFormat::UInt16;
  std::vector<MeshVertexAttribute> vertexAttributes;

  size_t getVertexSize() const;

  template <typename T> void getPipelineHash(T &hasher) const {
    hasher(primitiveType);
    hasher(windingOrder);
    hasher(indexFormat);
    hasher(vertexAttributes);
  }
};

/// <div rustbindgen opaque></div>
struct MeshContextData : public ContextData {
  MeshFormat format;
  size_t numVertices = 0;
  size_t numIndices = 0;
  WgpuHandle<WGPUBuffer> vertexBuffer;
  size_t vertexBufferLength = 0;
  WgpuHandle<WGPUBuffer> indexBuffer;
  size_t indexBufferLength = 0;

#if SH_GFX_CONTEXT_DATA_LABELS
  std::string label;
  std::string_view getLabel() const { return label; }
#endif

  MeshContextData() = default;
  MeshContextData(MeshContextData &&) = default;

  void init(std::string_view label) {
#if SH_GFX_CONTEXT_DATA_LABELS
    this->label = label;
#endif
#if SH_GFX_CONTEXT_DATA_LOG_LIFETIME
    SPDLOG_LOGGER_DEBUG(getContextDataLogger(), "Mesh {} data created", getLabel());
#endif
  }

#if SH_GFX_CONTEXT_DATA_LOG_LIFETIME
  ~MeshContextData() { SPDLOG_LOGGER_DEBUG(getContextDataLogger(), "Mesh {} data destroyed", getLabel()); }
#endif
};

/// <div rustbindgen opaque></div>
struct Mesh final {
  using ContextDataType = MeshContextData;

private:
  UniqueId id = getNextId();
  MeshFormat format;
  size_t numVertices = 0;
  size_t numIndices = 0;
  std::vector<uint8_t> vertexData;
  std::vector<uint8_t> indexData;
  size_t version{};

  friend struct gfx::UpdateUniqueId<Mesh>;

public:
  const MeshFormat &getFormat() const { return format; }

  size_t getNumVertices() const { return numVertices; }
  size_t getNumIndices() const { return numIndices; }
  const std::vector<uint8_t> &getVertexData() const { return vertexData; }
  const std::vector<uint8_t> &getIndexData() const { return indexData; }

  // This increments every time the mesh is updated
  size_t getVersion() const { return version; }

  // Updates mesh data with length in bytes
  void update(const MeshFormat &format, const void *inVertexData, size_t vertexDataLength, const void *inIndexData,
              size_t indexDataLength);
  void update(const MeshFormat &format, std::vector<uint8_t> &&vertexData,
              std::vector<uint8_t> &&indexData = std::vector<uint8_t>());

  UniqueId getId() const { return id; }
  MeshPtr clone() const;

  std::string_view getLabel() const { return "<unknown>"; }

  void pipelineHashCollect(detail::PipelineHashCollector &) const;

  void initContextData(Context &context, MeshContextData &contextData);
  void updateContextData(Context &context, MeshContextData &contextData);

protected:
  void calculateElementCounts(size_t vertexDataLength, size_t indexDataLength, size_t vertexSize, size_t indexSize);
  void update();

  static UniqueId getNextId();
};
} // namespace gfx

#endif /* A9FF2947_97BD_4662_A743_E2DB36A32FA2 */
