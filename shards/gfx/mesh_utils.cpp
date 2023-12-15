#include "mesh_utils.hpp"
#include "linalg.hpp"
#include "math.hpp"
#include <boost/container/small_vector.hpp>
#include <spdlog/spdlog.h>
#include "xxh3.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#include "MikkTSpace/mikktspace.c"
#pragma clang diagnostic pop

using namespace boost::container;

namespace gfx {

StorageType indexFormatToStorageType(IndexFormat fmt) {
  switch (fmt) {
  case IndexFormat::UInt16:
    return StorageType::UInt16;
  case IndexFormat::UInt32:
    return StorageType::UInt32;
  default:
    throw std::runtime_error("Invalid index format");
  }
}

IndexFormat storageTypeToIndexFormat(StorageType st) {
  switch (st) {
  case StorageType::UInt16:
    return IndexFormat::UInt16;
  case StorageType::UInt32:
    return IndexFormat::UInt32;
  default:
    throw std::runtime_error("Invalid storage type for index format");
  }
}

template <typename T> auto visitNumComponents(size_t size, T cb) {
  switch (size) {
  case 0:
    return cb(0);
  case 1:
    return cb(1);
  case 2:
    return cb(2);
  case 3:
    return cb(3);
  default:
    throw std::runtime_error(fmt::format("Invalid number of components: {}", size));
  }
}

template <typename T, bool OnlyRepresentable = true> auto visitStorageType(StorageType st, T cb) {
  if (OnlyRepresentable) {
    switch (st) {
    case StorageType::UInt8:
      return cb(uint8_t{});
    case StorageType::Int8:
      return cb(int8_t{});
    case StorageType::UInt16:
      return cb(uint16_t{});
    case StorageType::Int16:
      return cb(int16_t{});
    case StorageType::UInt32:
      return cb(uint32_t{});
    case StorageType::Int32:
      return cb(int32_t{});
    case StorageType::Float32:
      return cb(float{});
    default:
      throw std::runtime_error("Invalid or non-representable storage type");
    }
  } else {
    switch (st) {
    case StorageType::UInt8:
      return cb(uint8_t{});
    case StorageType::Int8:
      return cb(int8_t{});
    case StorageType::UNorm8:
      return cb(uint8_t{});
    case StorageType::SNorm8:
      return cb(int8_t{});
    case StorageType::UInt16:
      return cb(uint16_t{});
    case StorageType::Int16:
      return cb(int16_t{});
    case StorageType::UNorm16:
      return cb(uint16_t{});
    case StorageType::SNorm16:
      return cb(int16_t{});
    case StorageType::UInt32:
      return cb(uint32_t{});
    case StorageType::Int32:
      return cb(int32_t{});
    case StorageType::Float16:
      return cb(uint16_t{});
    case StorageType::Float32:
      return cb(float{});
    default:
      throw std::runtime_error("Invalid storage type");
    }
  }
}

struct AttribBuffer {
  uint8_t numComponents{1};
  StorageType type;
  std::vector<uint8_t> buf;
  size_t length{};

  size_t getSize() const { return numComponents * getStorageTypeSize(type); }

  void hash(XXH3_state_t &state, size_t index) const { XXH3_128bits_update(&state, buf.data() + getSize() * index, getSize()); }

  template <typename T, int N> void checkArgs() {
    if (numComponents != N)
      throw std::runtime_error("Invalid number of components");

    if constexpr (std::is_same_v<T, float>) {
      if (type != StorageType::Float32)
        throw std::runtime_error("Invalid storage type");
    } else if constexpr (std::is_same_v<T, uint32_t>) {
      if (type != StorageType::UInt32)
        throw std::runtime_error("Invalid storage type");
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      if (type != StorageType::UInt16)
        throw std::runtime_error("Invalid storage type");
    } else if constexpr (std::is_same_v<T, uint8_t>) {
      if (type != StorageType::UInt8)
        throw std::runtime_error("Invalid storage type");
    } else if constexpr (std::is_same_v<T, int8_t>) {
      if (type != StorageType::Int8)
        throw std::runtime_error("Invalid storage type");
    } else {
      static_assert(!std::is_same_v<T, T>, "Invalid type");
    }
  }

  template <typename T, int N, bool Checked = true> linalg::vec<T, N> read(size_t index) const {
    if constexpr (Checked) {
      checkArgs<T, N>();
    }

    return linalg::vec<T, N>((T *)(buf.data() + index * getSize()));
  }

  template <typename T, int N, bool Checked = true> void write(size_t index, const linalg::vec<T, N> &value) {
    if constexpr (Checked) {
      checkArgs<T, N>();
    }

    memcpy(buf.data() + index * getSize(), &value.x, getSize());
  }

  void initZero(StorageType type, uint8_t numComponents, size_t length) {
    this->type = type;
    this->numComponents = numComponents;
    this->length = length;
    this->buf.resize(length * getSize());
  }

  void initFrom(StorageType type, uint8_t numComponents, size_t length, size_t srcStride, const uint8_t *srcData) {
    initZero(type, numComponents, length);
    size_t attribSize = getSize();
    for (size_t i = 0; i < length; i++) {
      memcpy(this->buf.data() + attribSize * i, srcData + srcStride * i, attribSize);
    }
  }

  void initFromIndexedTriangleList(StorageType type, uint8_t numComponents, IndexFormat indexFormat, const uint8_t *indexData,
                                   size_t numIndices, size_t srcStride, const uint8_t *srcData) {
    initZero(type, numComponents, numIndices);
    size_t attribSize = getSize();
    if (indexFormat == IndexFormat::UInt16) {
      for (size_t i = 0; i < numIndices; i++) {
        uint32_t vertIndex = ((uint16_t *)indexData)[i];
        memcpy(this->buf.data() + attribSize * i, srcData + srcStride * vertIndex, attribSize);
      }
    } else {
      for (size_t i = 0; i < numIndices; i++) {
        uint32_t vertIndex = ((uint32_t *)indexData)[i];
        memcpy(this->buf.data() + attribSize * i, srcData + srcStride * vertIndex, attribSize);
      }
    }
  }

  void copyInto(uint8_t *dstData, size_t dstStride) const {
    size_t attribSize = getSize();
    for (size_t i = 0; i < length; i++) {
      memcpy(dstData + dstStride * i, buf.data() + attribSize * i, attribSize);
    }
  }

  static AttribBuffer generateIndexBuffer(size_t numTriangles) {
    AttribBuffer result;
    if (numTriangles < size_t(std::numeric_limits<uint16_t>::max())) {
      result.initZero(StorageType::UInt16, 1, numTriangles * 3);
      for (size_t t = 0; t < numTriangles * 3; t++) {
        ((uint16_t *)result.buf.data())[t] = t;
      }
    } else {
      result.initZero(StorageType::UInt32, 1, numTriangles * 3);
      for (size_t t = 0; t < numTriangles * 3; t++) {
        ((uint32_t *)result.buf.data())[t] = t;
      }
    }

    return result;
  }

  const uint8_t *data() const { return buf.data(); }
  uint8_t *data() { return buf.data(); }
};

MeshPtr generateMesh(std::optional<AttribBuffer> indexBuffer,
                     const small_vector<std::tuple<AttribBuffer *, FastString>, 16> &attributes) {
  MeshPtr newMesh = std::make_shared<Mesh>();
  MeshFormat format;
  format.primitiveType = PrimitiveType::TriangleList;
  format.windingOrder = WindingOrder::CCW;

  small_vector<size_t, 16> dstOffsets;
  size_t stride{};
  for (size_t i = 0; i < attributes.size(); i++) {
    auto &attrib = std::get<0>(attributes[i]);
    auto &name = std::get<1>(attributes[i]);
    format.vertexAttributes.push_back(MeshVertexAttribute{name.c_str(), attrib->numComponents, attrib->type});

    dstOffsets.push_back(stride);
    stride += attrib->numComponents * getStorageTypeSize(attrib->type);
  }

  std::vector<uint8_t> vertexBuffer;
  vertexBuffer.resize(stride * std::get<0>(attributes[0])->length);

  for (size_t i = 0; i < attributes.size(); i++) {
    auto &attrib = std::get<0>(attributes[i]);
    attrib->copyInto(vertexBuffer.data() + dstOffsets[i], stride);
  }

  if (indexBuffer.has_value()) {
    auto &ibAttribBuffer = indexBuffer.value();

    std::vector<uint8_t> indexBuffer;
    format.indexFormat = storageTypeToIndexFormat(ibAttribBuffer.type);
    indexBuffer.resize(getStorageTypeSize(ibAttribBuffer.type) * ibAttribBuffer.length);
    ibAttribBuffer.copyInto(indexBuffer.data(), getStorageTypeSize(ibAttribBuffer.type));

    newMesh->update(format, std::move(vertexBuffer), std::move(indexBuffer));
  } else {
    newMesh->update(format, std::move(vertexBuffer));
  }

  return newMesh;
}

// Generate surface orientation information in the form of a quaternion
// in case the mesh already contains at least a normal & tangent, these will be converted into quaternion form
// otherwise, the surface orientation quaternion will be generated from the normal and the texture coordinates (if available)
MeshPtr generateLocalBasisAttribute(MeshPtr mesh) {
  auto &srcFormat = mesh->getFormat();

  small_vector<size_t, 16> attributesToCopy;

  std::optional<size_t> positionIndex;
  std::optional<size_t> normalIndex;
  std::optional<size_t> texCoordIndex;

  small_vector<AttribBuffer, 16> srcAttributes;
  AttribBuffer indexBuffer;

  auto &indexData = mesh->getIndexData();
  auto &vertexData = mesh->getVertexData();
  size_t numIndices = mesh->getNumIndices();

  if (srcFormat.primitiveType != PrimitiveType::TriangleList) {
    throw std::runtime_error("Only triangle lists are supported");
  }

  if (numIndices > 0) {
    StorageType indexType = indexFormatToStorageType(srcFormat.indexFormat);
    indexBuffer.initFrom(indexType, 1, numIndices, getStorageTypeSize(indexType), indexData.data());
  } else {
    indexBuffer = AttribBuffer::generateIndexBuffer(numIndices / 3);
  }

  auto filterAttrib = [&](const MeshVertexAttribute &attrib, size_t index) {
    if (attrib.name == "position") {
      positionIndex = index;
    } else if (attrib.name == "normal") {
      normalIndex = index;
    } else if (attrib.name == "texCoord0") {
      texCoordIndex = index;
    } else if (attrib.name == "tangent" || attrib.name == "bitangent") {
      return false;
    }
    return true;
  };

  size_t srcStride = srcFormat.getVertexSize();
  size_t offset{};
  for (size_t i = 0; i < srcFormat.vertexAttributes.size(); i++) {
    auto &attrib = srcFormat.vertexAttributes[i];

    auto &buffer = srcAttributes.emplace_back();
    buffer.initFromIndexedTriangleList(attrib.type, attrib.numComponents, storageTypeToIndexFormat(indexBuffer.type),
                                       indexBuffer.data(), indexBuffer.length, srcStride, vertexData.data() + offset);

    offset += attrib.numComponents * getStorageTypeSize(attrib.type);
    if (filterAttrib(attrib, i)) {
      attributesToCopy.push_back(i);
    }
  }

  // Validate required attributes
  {
    if (!positionIndex.has_value()) {
      throw std::runtime_error("Mesh does not have a position attribute");
    }
    auto positionAttrib = srcFormat.vertexAttributes[positionIndex.value()];
    if (positionAttrib.type != StorageType::Float32 && positionAttrib.numComponents != 3) {
      throw std::runtime_error("position attribute must be float3");
    }

    if (!normalIndex) {
      throw std::runtime_error("Mesh does not have a normal attribute");
    }
    auto normalAttrib = srcFormat.vertexAttributes[normalIndex.value()];
    if (normalAttrib.type != StorageType::Float32 && normalAttrib.numComponents != 3) {
      throw std::runtime_error("normal attribute must be float3");
    }

    if (!texCoordIndex) {
      throw std::runtime_error("Mesh does not have a texCoord0 attribute");
    }
    auto texCoordAttrib = srcFormat.vertexAttributes[texCoordIndex.value()];
    if (texCoordAttrib.type != StorageType::Float32 && texCoordAttrib.numComponents != 2) {
      throw std::runtime_error("texCoord0 attribute must be float2");
    }
  }

  if (indexData.size() == 0) {
    throw std::runtime_error("Mesh does not have an index buffer");
  }

  AttribBuffer tangentBuffer;
  tangentBuffer.initZero(StorageType::Float32, 4, numIndices);

  struct Context {
    SMikkTSpaceContext ctx;
    const AttribBuffer &indexBuffer;
    const AttribBuffer &positionBuffer;
    const AttribBuffer &normalBuffer;
    const AttribBuffer &texCoordBuffer;
    AttribBuffer &tangentBuffer;
  } context{
      .indexBuffer = indexBuffer,
      .positionBuffer = srcAttributes[positionIndex.value()],
      .normalBuffer = srcAttributes[normalIndex.value()],
      .texCoordBuffer = srcAttributes[texCoordIndex.value()],
      .tangentBuffer = tangentBuffer,
  };

  SMikkTSpaceInterface iface{
      // Returns the number of faces (triangles/quads) on the mesh to be processed.
      .m_getNumFaces = [](const SMikkTSpaceContext *ctx) -> int { return ((Context *)ctx)->indexBuffer.length / 3; },
      .m_getNumVerticesOfFace = [](const SMikkTSpaceContext *ctx, const int iFace) -> int { return 3; },
  };
  iface.m_getPosition = [](const SMikkTSpaceContext *ctx, float fvPosOut[], const int iFace, const int iVert) {
    auto vec = ((Context *)ctx)->positionBuffer.read<float, 3, false>(iFace * 3 + iVert);
    fvPosOut[0] = float(vec.x);
    fvPosOut[1] = float(vec.y);
    fvPosOut[2] = float(vec.z);
  };
  iface.m_getNormal = [](const SMikkTSpaceContext *ctx, float fvPosOut[], const int iFace, const int iVert) {
    auto vec = ((Context *)ctx)->normalBuffer.read<float, 3, false>(iFace * 3 + iVert);
    fvPosOut[0] = float(vec.x);
    fvPosOut[1] = float(vec.y);
    fvPosOut[2] = float(vec.z);
  };
  iface.m_getTexCoord = [](const SMikkTSpaceContext *ctx, float fvPosOut[], const int iFace, const int iVert) {
    auto vec = ((Context *)ctx)->texCoordBuffer.read<float, 2, false>(iFace * 3 + iVert);
    // NOTE: Texcoord is flipped to match glTF space
    //   reference: https://github.com/mrdoob/three.js/issues/25263
    fvPosOut[0] = fmod(float(vec.x), 1.0f);
    fvPosOut[1] = 1.0f - fmod(float(vec.y), 1.0f);
  };

  iface.m_setTSpaceBasic = [](const SMikkTSpaceContext *ctx, const float fvTangent[], const float fSign, const int iFace,
                              const int iVert) {
    float3 x = float3(fvTangent);
    auto &tangentBuffer = ((Context *)ctx)->tangentBuffer;
    tangentBuffer.write(iFace * 3 + iVert, float4(x, fSign));
  };

  context.ctx.m_pInterface = &iface;
  genTangSpaceDefault(&context.ctx);

  small_vector<std::tuple<AttribBuffer *, FastString>, 16> outAttribs;
  for (size_t i : attributesToCopy) {
    outAttribs.push_back({&srcAttributes[i], srcFormat.vertexAttributes[i].name});
  }
  outAttribs.push_back({&tangentBuffer, "tangent"});
  return generateMesh(std::nullopt, outAttribs);
}
} // namespace gfx