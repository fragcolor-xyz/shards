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

inline bool isFlippedCoordinateSpace(float3 right, float3 forward, float3 up) {
  float3 expectedForward = linalg::normalize(linalg::cross(right, up));
  if (linalg::dot(forward, expectedForward) < 0) {
    return true;
  }
  return false;
}

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
                     const small_vector<std::tuple<AttribBuffer *, std::string>, 16> &attributes) {
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
  size_t numVertices = mesh->getNumVertices();
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

  // (float*)(*((boost::container::vector<gfx::AttribBuffer,boost::container::small_vector_allocator<gfx::AttribBuffer,boost::container::new_allocator<void>,void>,void>*)&srcAttributes))[2].buf.data(),100
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

  // MeshFormat dstFormat = srcFormat;
  // dstFormat.vertexAttributes.clear();

  // size_t dstStride{};
  // small_vector<size_t, 16> dstAttribOffsets;

  // Directly copy existing attributes
  // for (size_t i = 0; i < attributesToCopy.size(); i++) {
  //   auto &attrib = srcFormat.vertexAttributes[attributesToCopy[i]];
  //   dstFormat.vertexAttributes.push_back(attrib);
  //   dstAttribOffsets.push_back(dstStride);
  //   dstStride += attrib.numComponents * getStorageTypeSize(attrib.type);
  // }

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
    //  decltype(type)
    auto vec = ((Context *)ctx)->positionBuffer.read<float, 3, false>(iFace * 3 + iVert);
    fvPosOut[0] = float(vec.x);
    fvPosOut[1] = float(vec.y);
    fvPosOut[2] = float(vec.z);
  };
  iface.m_getNormal = [](const SMikkTSpaceContext *ctx, float fvPosOut[], const int iFace, const int iVert) {
    //  decltype(type)
    auto vec = ((Context *)ctx)->normalBuffer.read<float, 3, false>(iFace * 3 + iVert);
    fvPosOut[0] = float(vec.x);
    fvPosOut[1] = float(vec.y);
    fvPosOut[2] = float(vec.z);
  };
  iface.m_getTexCoord = [](const SMikkTSpaceContext *ctx, float fvPosOut[], const int iFace, const int iVert) {
    //  decltype(type)
    auto vec = ((Context *)ctx)->texCoordBuffer.read<float, 2, false>(iFace * 3 + iVert);
    fvPosOut[0] = fmod(float(vec.x), 1.0f);
    fvPosOut[1] = fmod(float(vec.y), 1.0f);
  };

  iface.m_setTSpaceBasic = [](const SMikkTSpaceContext *ctx, const float fvTangent[], const float fSign, const int iFace,
                              const int iVert) {
    auto z = linalg::normalize(((Context *)ctx)->normalBuffer.read<float, 3, false>(iFace * 3 + iVert));
    float3 x = float3(fvTangent);

    // if (linalg::length2(y) == 0.0f) {
    //   y = -generateTangent(z); // prevent NAN
    // }

    // float3 y = linalg::normalize(linalg::cross(z, x));
    // x = linalg::normalize(linalg::cross(y, z));

    // if (isFlippedCoordinateSpace(x, z, y)) {
    //   SPDLOG_INFO("flipped");
    // }

    // float d0 = linalg::dot(x, y);
    // float d1 = linalg::dot(y, z);
    // float d2 = linalg::dot(z, x);
    // if (!isRoughlyEqual(d0, 0.0f) || !isRoughlyEqual(d1, 0.0f) || !isRoughlyEqual(d2, 0.0f)) {
    //   SPDLOG_INFO("non-orthogonal");
    // }

    // float4 qbase = linalg::normalize(linalg::rotation_quat(float3x3(x, y, z)));

    // auto &qbaseBuffer = ((Context *)ctx)->qbaseBuffer;
    // qbaseBuffer.write(iFace * 3 + iVert, qbase);
    auto &tangentBuffer = ((Context *)ctx)->tangentBuffer;
    tangentBuffer.write(iFace * 3 + iVert, float4(x, fSign));
  };
  // iface.m_setTSpace = [](const SMikkTSpaceContext *ctx, const float fvTangent[], const float fvBiTangent[], const float fMagS,
  //                        const float fMagT, const tbool bIsOrientationPreserving, const int iFace, const int iVert) {
  //   auto z = linalg::normalize(((Context *)ctx)->normalBuffer.read<float, 3, false>(iFace * 3 + iVert));

  //   float3 x(fvTangent);
  //   float3 y(fvBiTangent);

  //   float fSign = bIsOrientationPreserving ? 1.0f : (-1.0f);
  //   float3 bitangent = fSign * cross(z, x);
  //   // float3 y = linalg::normalize(fSign * linalg::cross(z, x));
  //   // x = linalg::normalize(linalg::cross(y, z));
  //   // float4 qbase = linalg::normalize(linalg::rotation_quat(float3x3(x, y, z)));
  //   float4 qbase = float4(x * fMagS, 0.0);

  //   auto &qbaseBuffer = ((Context *)ctx)->qbaseBuffer;
  //   qbaseBuffer.write(iFace * 3 + iVert, qbase);
  // };
  context.ctx.m_pInterface = &iface;
  genTangSpaceDefault(&context.ctx);

  // if (normalIndex) {
  //   auto& posBuffer = srcAttributes[positionIndex.value()];
  //   auto& uvBuffer = srcAttributes[positionIndex.value()];
  //   auto& normalBuffer = srcAttributes[positionIndex.value()];
  //   // Normals are either present as vertex attributes or approximated.
  //   for (size_t ti = 0; ti < numIndices / 3; ti++) {
  //     // vec2 UV = getNormalUV();
  //     // vec2 uv_dx = dFdx(UV);
  //     // vec2 uv_dy = dFdy(UV);

  //     // if (length(uv_dx) + length(uv_dy) <= 1e-6) {
  //     //   uv_dx = vec2(1.0, 0.0);
  //     //   uv_dy = vec2(0.0, 1.0);
  //     // }

  //     // vec3 t_ = (uv_dy.t * dFdx(v_Position) - uv_dx.t * dFdy(v_Position)) / (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t);

  //     // vec3 n, t, b, ng;
  //     float3 positions[3];
  //     float2 uvs[3];
  //     for (size_t i = 0; i < 3; i++) {
  //       positions[i] =
  //     }
  //     ng = normalize(v_Normal);
  //     t = normalize(t_ - ng * dot(ng, t_));
  //     b = cross(ng, t);
  //   }
  // } else {
  //   ng = normalize(cross(dFdx(v_Position), dFdy(v_Position)));
  //   t = normalize(t_ - ng * dot(ng, t_));
  //   b = cross(ng, t);
  // }

  small_vector<std::tuple<AttribBuffer *, std::string>, 16> outAttribs;
  for (size_t i : attributesToCopy) {
    outAttribs.push_back({&srcAttributes[i], srcFormat.vertexAttributes[i].name});
  }
  outAttribs.push_back({&tangentBuffer, "tangent"});
  return generateMesh(std::nullopt, outAttribs);
  // MeshPtr newMesh = std::make_shared<Mesh>();
  // newMesh->update(dstFormat, std::move(newVertexData), std::vector<uint8_t>(indexData));
  // return newMesh;

  // auto &srcFormat = mesh->getFormat();

  // small_vector<size_t, 16> srcAttribOffsets;
  // small_vector<size_t, 16> srcAttribs;
  // small_vector<size_t, 16> dstAttribs;

  // size_t i{};
  // std::optional<size_t> normalIndex;
  // std::optional<size_t> tangentIndex;
  // std::optional<size_t> texCoordIndex;
  // std::optional<size_t> positionIndex;
  // auto filterAttrib = [&](const MeshVertexAttribute &attrib) {
  //   if (attrib.name == "position") {
  //     positionIndex = i;
  //     return true;
  //   }
  //   if (attrib.name == "normal") {
  //     normalIndex = i;
  //     return true;
  //   }
  //   if (attrib.name == "texCoord0") {
  //     texCoordIndex = i;
  //     return true;
  //   }
  //   if (attrib.name == "tangent") {
  //     tangentIndex = i;
  //     return false;
  //   }
  //   if (attrib.name == "binormal" || attrib.name == "bitangent") {
  //     return false;
  //   }
  //   return true;
  // };

  // i = 0;
  // size_t offset{};
  // for (auto &attrib : srcFormat.vertexAttributes) {
  //   srcAttribOffsets.push_back(offset);
  //   offset += attrib.numComponents * getStorageTypeSize(attrib.type);
  //   if (filterAttrib(attrib)) {
  //     srcAttribs.push_back(i);
  //     dstAttribs.push_back(i);
  //   }
  //   i++;
  // }

  // if (!positionIndex.has_value()) {
  //   throw std::runtime_error("Mesh does not have a position attribute");
  // }

  // auto &indexData = mesh->getIndexData();
  // auto &vertexData = mesh->getVertexData();
  // size_t srcStride = srcFormat.getVertexSize();
  // auto readFloatAttrib = [&](float *out, size_t attribIndex, size_t index) {
  //   size_t vertIdx = srcFormat.indexFormat == IndexFormat::UInt16 ? ((uint16_t *)indexData.data())[index]
  //                                                                 : ((uint32_t *)indexData.data())[index];
  //   const uint8_t *srcData = vertexData.data() + srcStride * vertIdx + srcAttribOffsets[attribIndex];
  //   memcpy(out, srcData, sizeof(float) * srcFormat.vertexAttributes[attribIndex].numComponents);
  // };

  // auto readFloatTriangleAttribs = [&](float *first, size_t numElements, size_t attribIndex, size_t startingIndex) {
  //   for (size_t i = 0; i < 3; i++) {
  //     readFloatAttrib(first + i * numElements, attribIndex, startingIndex + i);
  //   }
  // };

  // size_t numIndices = mesh->getNumIndices();
  // if (numIndices > 0) {
  //   if (srcFormat.primitiveType == PrimitiveType::TriangleList) {
  //     for (size_t i = 0; i < numIndices; i += 3) {
  //       float3 positions[3];
  //       readFloatTriangleAttribs(&positions[0].x, 3, positionIndex.value(), i);

  //       if (normalIndex && texCoordIndex) {
  //         float3 normal[3];
  //         readFloatTriangleAttribs(&normal[0].x, 3, normalIndex.value(), i);

  //         float2 uv[3];
  //         readFloatTriangleAttribs(&uv[0].x, 2, texCoordIndex.value(), i);
  //         // float3 t0 =
  //       } else {
  //         throw std::runtime_error("Mesh does not have a normal or texCoord0 attribute to derive tangent from");
  //       }
  //     }
  //   } else {
  //     throw std::runtime_error("Unsupported primitive type");
  //   }
  // } else {
  //   throw std::runtime_error("generateTangentInfo only supports indexed meshes");
  // }

  // MeshFormat dstFormat = srcFormat;
  // dstFormat.vertexAttributes.clear();
}
} // namespace gfx