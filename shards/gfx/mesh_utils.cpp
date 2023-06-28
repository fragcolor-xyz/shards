#include "mesh_utils.hpp"
#include "linalg.hpp"
#include <boost/container/small_vector.hpp>
#include <spdlog/spdlog.h>
#include <Eigen/Core>
#include <igl/local_basis.h>
// #include 

using namespace boost::container;

namespace gfx {

// Generate surface orientation information in the form of a quaternion
// in case the mesh already contains at least a normal & tangent, these will be converted into quaternion form
// otherwise, the surface orientation quaternion will be generated from the normal and the texture coordinates (if available)
MeshPtr generateLocalBasisAttribute(MeshPtr mesh) {
  auto &srcFormat = mesh->getFormat();

  small_vector<size_t, 16> srcAttribOffsets;
  small_vector<size_t, 16> attributesToCopy;

  std::optional<size_t> positionIndex;
  std::optional<size_t> normalIndex;
  auto filterAttrib = [&](const MeshVertexAttribute &attrib, size_t index) {
    if (attrib.name == "position") {
      positionIndex = index;
    } else if (attrib.name == "normal") {
      normalIndex = index;
      return false;
    } else if (attrib.name == "tangent" || attrib.name == "bitangent") {
      return false;
    }
    return true;
  };

  size_t srcStride{};
  for (size_t i = 0; i < srcFormat.vertexAttributes.size(); i++) {
    auto &attrib = srcFormat.vertexAttributes[i];
    srcAttribOffsets.push_back(srcStride);
    srcStride += attrib.numComponents * getStorageTypeSize(attrib.type);
    if (filterAttrib(attrib, i)) {
      attributesToCopy.push_back(i);
    }
  }

  if (!positionIndex.has_value()) {
    throw std::runtime_error("Mesh does not have a position attribute");
  }

  auto &indexData = mesh->getIndexData();
  auto &vertexData = mesh->getVertexData();
  size_t numVertices = mesh->getNumVertices();
  size_t numIndices = mesh->getNumIndices();

  if (indexData.size() == 0) {
    throw std::runtime_error("Mesh does not have an index buffer");
  }

  const uint8_t *srcData = vertexData.data() + srcAttribOffsets[positionIndex.value()];
  Eigen::Map<Eigen::MatrixX3f, 0, Eigen::OuterStride<>> vertices((float *)srcData, numVertices, 3,
                                                                 Eigen::OuterStride<>(srcStride));

  using IMat16 = Eigen::Matrix<uint16_t, Eigen::Dynamic, 3>;
  using IMat32 = Eigen::Matrix<uint32_t, Eigen::Dynamic, 3>;
  Eigen::Matrix<uint32_t, Eigen::Dynamic, 3> I;
  if (srcFormat.indexFormat == IndexFormat::UInt16) {
    I = Eigen::Map<IMat16>((uint16_t *)indexData.data(), mesh->getNumIndices() / 3, 3).cast<uint32_t>();
  } else {
    I = Eigen::Map<IMat32>((uint32_t *)indexData.data(), mesh->getNumIndices() / 3, 3);
  }

  Eigen::MatrixX3f V = vertices;
  Eigen::MatrixX3f F1, F2, F3;
  igl::local_basis(V, I, F1, F2, F3);
  SPDLOG_INFO("Test");

  MeshFormat dstFormat = srcFormat;
  dstFormat.vertexAttributes.clear();

  size_t dstStride{};
  small_vector<size_t, 16> dstAttribOffsets;

  for (size_t i = 0; i < attributesToCopy.size(); i++) {
    auto &attrib = srcFormat.vertexAttributes[attributesToCopy[i]];
    dstFormat.vertexAttributes.push_back(attrib);
    dstAttribOffsets.push_back(dstStride);
    dstStride += attrib.numComponents * getStorageTypeSize(attrib.type);
  }

  // The generated attribute
  size_t qbaseAttribIndex{};
  {
    qbaseAttribIndex = dstFormat.vertexAttributes.size();
    auto &attrib = dstFormat.vertexAttributes.emplace_back();
    attrib.name = "qbase";
    attrib.numComponents = 4;
    attrib.type = StorageType::Float32;
    dstAttribOffsets.push_back(dstStride);
    dstStride += attrib.numComponents * getStorageTypeSize(attrib.type);
  }

  std::vector<uint8_t> newVertexData;
  newVertexData.resize(dstStride * numVertices);
  for (size_t c = 0; c < dstFormat.vertexAttributes.size(); c++) {
    auto &attrib = dstFormat.vertexAttributes[c];
    if (c == qbaseAttribIndex)
      continue;
    for (size_t i = 0; i < numVertices; i++) {
      uint8_t *dstData = newVertexData.data() + dstStride * i + dstAttribOffsets[c];
      const uint8_t *srcData = vertexData.data() + srcStride * i + srcAttribOffsets[attributesToCopy[c]];
      memcpy(dstData, srcData, attrib.numComponents * getStorageTypeSize(attrib.type));
    }
  }

  if (normalIndex) {
    if (srcFormat.vertexAttributes[normalIndex.value()].type != StorageType::Float32) {
      throw std::runtime_error("Normal attribute must be float32");
    }
  }

  if (normalIndex) {
    for (size_t vertIndex = 0; vertIndex < numVertices; vertIndex++) {
      float3 normal((float *)(vertexData.data() + srcStride * vertIndex + srcAttribOffsets[normalIndex.value()]));

      float3 tx = float3(normal.y, -normal.x, normal.z);
      float3 tz = linalg::normalize(linalg::cross(tx, normal));
      tx = linalg::normalize(linalg::cross(normal, tz));
      float4 qbase = linalg::rotation_quat(float3x3(tx, -tz, normal));

      uint8_t *dstData = newVertexData.data() + dstStride * vertIndex + dstAttribOffsets[qbaseAttribIndex];
      memcpy(dstData, &qbase.x, 4 * sizeof(float));
    }
  } else {
    size_t numTriangles = numIndices / 3;
    for (size_t i = 0; i < numTriangles; i++) {
      float3 x(F1.row(i).data());
      float3 y(F2.row(i).data());
      float3 z(F3.row(i).data());
      float4 qbase = linalg::rotation_quat(float3x3(x, y, z));
      for (size_t j = 0; j < 3; j++) {
        size_t vertIndex = I.row(i)(j);
        uint8_t *dstData = newVertexData.data() + dstStride * vertIndex + dstAttribOffsets[qbaseAttribIndex];
        memcpy(dstData, &qbase.x, 4 * sizeof(float));
      }
    }
  }
  // for (size_t i = 0; i < numVertices; i++) {
  //   float3 normal{};
  //   if (normalIndex)
  //     const float *srcNormal = (float *)(vertexData.data() + srcStride * i + srcAttribOffsets[normalIndex.value()]);
  //   // normal.
  //   // srcData.
  //   // float* srcNormal = normalIndex.value();
  //   uint8_t *dstData = newVertexData.data() + dstStride * i + dstAttribOffsets[qbaseAttribIndex];
  //   float3 x(F1.row(i).data());
  //   float3 y(F2.row(i).data());
  //   float3 z(F3.row(i).data());
  //   float4 qbase = linalg::rotation_quat(float3x3(x, y, z));
  //   memcpy(dstData, &qbase.x, 4 * sizeof(float));
  // }

  MeshPtr newMesh = std::make_shared<Mesh>();
  newMesh->update(dstFormat, std::move(newVertexData), std::vector<uint8_t>(indexData));
  return newMesh;

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