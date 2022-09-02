#include "features/wireframe.hpp"
#include "drawable.hpp"
#include "linalg.hpp"
#include "mesh.hpp"
#include "transform_updater.hpp"
#include "wireframe.hpp"
#include <stdexcept>

namespace gfx {
MeshPtr WireframeMeshGenerator::generate() {
  if (!mesh)
    throw std::logic_error("Input mesh required");

  auto &format = mesh->getFormat();
  auto indicesPtr = mesh->getIndexData().data();
  auto verticesPtr = mesh->getVertexData().data();
  if (mesh->getNumIndices() == 0)
    return mesh;

  size_t positionOffset = ~0;
  for (size_t i = 0; i < format.vertexAttributes.size(); i++) {
    auto &attr = format.vertexAttributes[i];
    if (attr.name == "position") {
      positionOffset = i;
      break;
    }
  }

  size_t stride = format.getVertexSize();

  if (positionOffset == ~0)
    throw std::runtime_error("Mesh has no vertex positions");

  size_t numIndices = mesh->getNumIndices();

  std::function<void(size_t base, size_t(&)[3])> getTriangle;
  if (format.indexFormat == IndexFormat::UInt16) {
    getTriangle = [&](size_t baseIndex, size_t(&indices)[3]) {
      indices[0] = ((uint16_t *)indicesPtr)[baseIndex];
      indices[1] = ((uint16_t *)indicesPtr)[baseIndex + 1];
      indices[2] = ((uint16_t *)indicesPtr)[baseIndex + 2];
    };
  } else {
    getTriangle = [&](size_t baseIndex, size_t(&indices)[3]) {
      indices[0] = ((uint32_t *)indicesPtr)[baseIndex];
      indices[1] = ((uint32_t *)indicesPtr)[baseIndex + 1];
      indices[2] = ((uint32_t *)indicesPtr)[baseIndex + 2];
    };
  }

  struct Vert {
    float position[3];
    void setPosition(const float3 &pos) {
      position[0] = pos.x;
      position[1] = pos.y;
      position[2] = pos.z;
    }
  };

  auto readPositions = [&](size_t(&indices)[3], Vert *outVerts) {
    for (size_t i = 0; i < 3; i++) {
      const uint8_t *srcBasePtr = verticesPtr + (stride * indices[i]);
      const float3 *srcPosPtr = (float3 *)(srcBasePtr + positionOffset);
      outVerts[i].setPosition(*srcPosPtr);
    }
  };

  std::vector<Vert> verts;

  if (format.primitiveType == PrimitiveType::TriangleList) {
    size_t numTriangles = numIndices / 3;
    for (size_t i = 0; i < numTriangles; i++) {
      size_t triangle[3];
      getTriangle(i * 3, triangle);

      size_t outIndex = verts.size();
      verts.resize(outIndex + 3);
      readPositions(triangle, &verts[outIndex]);
    }
  } else {
    throw std::runtime_error("Unsupported primitive type");
  }

  MeshFormat newFormat{
      .primitiveType = PrimitiveType::TriangleList,
      .windingOrder = format.windingOrder,
      .indexFormat = IndexFormat::UInt16,
      .vertexAttributes =
          {
              MeshVertexAttribute("position", 3),
          },
  };
  auto newMesh = std::make_shared<Mesh>();
  newMesh->update(newFormat, std::move((std::vector<uint8_t> &)verts));
  return newMesh;
}

WireframeRenderer::WireframeRenderer(bool showBackfaces) { wireframeFeature = features::Wireframe::create(showBackfaces); }

void WireframeRenderer::overlayWireframe(DrawQueue &queue, DrawablePtr drawable) {
  Mesh *meshPtr = drawable->mesh.get();
  auto it = meshCache.find(meshPtr);
  if (it == meshCache.end()) {
    WireframeMeshGenerator meshGenerator;
    meshGenerator.mesh = drawable->mesh;
    MeshCacheEntry entry{
        .wireMesh = meshGenerator.generate(),
        .weakMesh = drawable->mesh,
    };

    it = meshCache.insert_or_assign(meshPtr, entry).first;
  }

  auto clone = drawable->clone();
  WireframeMeshGenerator meshGenerator;
  meshGenerator.mesh = drawable->mesh;
  clone->mesh = it->second.wireMesh;
  clone->features.push_back(wireframeFeature);
  queue.add(clone);
}

void WireframeRenderer::overlayWireframe(DrawQueue &queue, DrawableHierarchyPtr drawableHierarchy) {
  TransformUpdaterCollector collector;
  collector.collector = [&](DrawablePtr drawable) { overlayWireframe(queue, drawable); };
  collector.update(drawableHierarchy);
}
} // namespace gfx