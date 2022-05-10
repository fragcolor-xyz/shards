#include "mesh.hpp"
#include "context.hpp"
#include <cassert>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

namespace gfx {

void Mesh::update(const MeshFormat &format, const void *inVertexData, size_t vertexDataLength, const void *inIndexData,
                  size_t indexDataLength) {
  this->format = format;

  vertexData.resize(vertexDataLength);
  memcpy(vertexData.data(), inVertexData, vertexDataLength);

  indexData.resize(indexDataLength);
  memcpy(indexData.data(), inIndexData, indexDataLength);

  update();
}

void Mesh::update(const MeshFormat &format, std::vector<uint8_t> &&vertexData, std::vector<uint8_t> &&indexData) {
  this->format = format;
  this->vertexData = std::move(vertexData);
  this->indexData = std::move(indexData);

  update();
}

void Mesh::update() {
  size_t vertexSize = 0;
  for (auto &attr : format.vertexAttributes) {
    size_t typeSize = getVertexAttributeTypeSize(attr.type);
    vertexSize += attr.numComponents * typeSize;
  }

  numVertices = vertexData.size() / vertexSize;
  assert(numVertices * vertexSize == vertexData.size());

  size_t indexSize = getIndexFormatSize(format.indexFormat);
  numIndices = indexData.size() / indexSize;
  assert(numIndices * indexSize == indexData.size());

  // This causes the GPU data to be recreated the next time it is requested
  contextData.reset();
}

void Mesh::initContextData(Context &context, MeshContextData &contextData) {
  WGPUDevice device = context.wgpuDevice;
  assert(device);

  contextData.format = format;
  contextData.numIndices = numIndices;
  contextData.numVertices = numVertices;

  WGPUBufferDescriptor desc = {};
  desc.size = vertexData.size();
  desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  contextData.vertexBuffer = wgpuDeviceCreateBuffer(device, &desc);
  contextData.vertexBufferLength = desc.size;

  wgpuQueueWriteBuffer(context.wgpuQueue, contextData.vertexBuffer, 0, vertexData.data(), vertexData.size());

  if (indexData.size() > 0) {
    desc.size = indexData.size();
    desc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    contextData.indexBuffer = wgpuDeviceCreateBuffer(device, &desc);
    contextData.indexBufferLength = desc.size;

    wgpuQueueWriteBuffer(context.wgpuQueue, contextData.indexBuffer, 0, indexData.data(), indexData.size());
  }
}

} // namespace gfx
