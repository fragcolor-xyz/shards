
#pragma once

#include "context_data.hpp"
#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "linalg/linalg.h"
#include <string>

namespace gfx {

struct MeshVertexAttribute {
  std::string name;
  uint8_t numComponents;
  VertexAttributeType type;

  MeshVertexAttribute() = default;
  MeshVertexAttribute(const char *name, uint8_t numComponents, VertexAttributeType type = VertexAttributeType::Float32)
      : name(name), numComponents(numComponents), type(type) {}

  // Compares everthing except for the name
  bool isSameDataFormat(const MeshVertexAttribute &other) const {
    return numComponents == other.numComponents && type == other.type;
  }

  template <typename T> void hashStatic(T &hasher) const {
    hasher(name);
    hasher(numComponents);
    hasher(type);
  }
};

struct MeshFormat {
  PrimitiveType primitiveType = PrimitiveType::TriangleList;
  WindingOrder windingOrder = WindingOrder::CCW;
  IndexFormat indexFormat = IndexFormat::UInt16;
  std::vector<MeshVertexAttribute> vertexAttributes;

  template <typename T> void hashStatic(T &hasher) const {
    hasher(primitiveType);
    hasher(windingOrder);
    hasher(indexFormat);
    hasher(vertexAttributes);
  }
};

struct MeshContextData : public ContextData {
  MeshFormat format;
  size_t numVertices = 0;
  size_t numIndices = 0;
  WGPUBuffer vertexBuffer = nullptr;
  size_t vertexBufferLength = 0;
  WGPUBuffer indexBuffer = nullptr;
  size_t indexBufferLength = 0;

  ~MeshContextData() { releaseConditional(); }
  void release() {
    WGPU_SAFE_RELEASE(wgpuBufferRelease, vertexBuffer);
    WGPU_SAFE_RELEASE(wgpuBufferRelease, indexBuffer);
  }
};

struct Mesh final : public TWithContextData<MeshContextData> {
private:
  MeshFormat format;
  size_t numVertices = 0;
  size_t numIndices = 0;
  std::vector<uint8_t> vertexData;
  std::vector<uint8_t> indexData;

public:
  Mesh() {}
  ~Mesh() {}

  const MeshFormat &getFormat() const { return format; }

  size_t getNumVertices() const { return numVertices; }
  size_t getNumIndices() const { return numIndices; }

  // Updates mesh data with length in bytes
  void update(const MeshFormat &format, const void *inVertexData, size_t vertexDataLength, const void *inIndexData,
              size_t indexDataLength);
  void update(const MeshFormat &format, std::vector<uint8_t> &&vertexData, std::vector<uint8_t> &&indexData);

protected:
  void update();
  void initContextData(Context &context, MeshContextData &contextData);
  void releaseContextData();
};
using MeshPtr = std::shared_ptr<Mesh>;

} // namespace gfx
