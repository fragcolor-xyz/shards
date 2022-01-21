#include "mesh.hpp"
#include "context.hpp"
#include <cassert>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

namespace gfx {

void Mesh::update(const Format &format, const void *inVertexData, size_t vertexDataLength, const void *inIndexData, size_t indexDataLength) {
	this->format = format;

	vertexData.resize(vertexDataLength);
	memcpy(vertexData.data(), inVertexData, vertexDataLength);

	indexData.resize(indexDataLength);
	memcpy(indexData.data(), inIndexData, indexDataLength);

	size_t vertexSize = 0;
	for (auto &attr : format.vertexAttributes) {
		size_t typeSize = getVertexAttributeTypeSize(attr.type);
		vertexSize += attr.numComponents * typeSize;
	}

	numVertices = vertexDataLength / vertexSize;
	assert(numVertices * vertexSize == vertexDataLength);

	size_t indexSize = getIndexFormatSize(format.indexFormat);
	numIndices = indexDataLength / indexSize;
	assert(numIndices * indexSize == indexDataLength);

	releaseContextDataCondtional();
}

void Mesh::createContextData() {
	WGPUDevice device = context->wgpuDevice;
	assert(device);

	auto validation = context->pushErrorScope();

	WGPUBufferDescriptor desc = {};
	desc.size = vertexData.size();
	desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
	contextData.vertexBuffer = wgpuDeviceCreateBuffer(device, &desc);
	contextData.vertexBufferLength = desc.size;

	auto vertexCopyBuffer = context->pushCopyBuffer();
	vertexCopyBuffer->data = vertexData;
	wgpuQueueWriteBuffer(context->wgpuQueue, contextData.vertexBuffer, 0, vertexCopyBuffer->data.data(), vertexCopyBuffer->data.size());

	if (indexData.size() > 0) {
		desc.size = indexData.size();
		desc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
		contextData.indexBuffer = wgpuDeviceCreateBuffer(device, &desc);
		contextData.indexBufferLength = desc.size;

		auto indexCopyBuffer = context->pushCopyBuffer();
		indexCopyBuffer->data = indexData;
		wgpuQueueWriteBuffer(context->wgpuQueue, contextData.indexBuffer, 0, indexCopyBuffer->data.data(), indexCopyBuffer->data.size());
	}

	validation.pop([](WGPUErrorType type, const char *msg) { spdlog::error("Failed to create buffer ({}): {}", magic_enum::enum_name(type), msg); });
}

void Mesh::releaseContextData() {
	WGPU_SAFE_RELEASE(wgpuBufferRelease, contextData.vertexBuffer);
	WGPU_SAFE_RELEASE(wgpuBufferRelease, contextData.indexBuffer);
}
} // namespace gfx