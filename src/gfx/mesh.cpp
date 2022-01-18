#include "mesh.hpp"
#include "context.hpp"
#include <cassert>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

namespace gfx {

void Mesh::update(const Format &format, const void *inVertexData, size_t vertexSize, const void *inIndexData, size_t indexSize) {
	this->format = format;
	vertexData.resize(vertexSize);
	memcpy(vertexData.data(), inVertexData, vertexSize);
	indexData.resize(indexSize);
	memcpy(indexData.data(), inIndexData, indexSize);

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

	auto vertexCopyBuffer = context->pushCopyBuffer();
	vertexCopyBuffer->data = vertexData;
	wgpuQueueWriteBuffer(context->wgpuQueue, contextData.vertexBuffer, 0, vertexCopyBuffer->data.data(), vertexCopyBuffer->data.size());

	if (indexData.size() > 0) {
		desc.size = indexData.size();
		desc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
		contextData.indexBuffer = wgpuDeviceCreateBuffer(device, &desc);

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