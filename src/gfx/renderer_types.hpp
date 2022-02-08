#pragma once
#include "gfx_wgpu.hpp"
#include <cassert>
#include <vector>

namespace gfx {

// Wraps an object that is swapped per frame for double+ buffered rendering
template <typename TInner, size_t MaxSize> struct Swappable {
	TInner elems[MaxSize];
	TInner &get(size_t frameNumber) {
		assert(frameNumber <= MaxSize);
		return elems[frameNumber];
	}
	TInner &operator()(size_t frameNumber) { return get(frameNumber); }
};

// Wraps a WGPUBuffer with resize operations allowing buffer reuse
struct DynamicWGPUBuffer {
private:
	size_t capacity{};
	WGPUBuffer buffer{};
	WGPUBufferUsageFlags usage{};

public:
	// Resizes the buffer, leaves the data within it undefined
	void resize(WGPUDevice device, size_t size, WGPUBufferUsageFlags usage, const char *label = nullptr) {
		if (this->usage != usage) {
			cleanup();
			this->usage = usage;
		}

		if (size > capacity) {
			cleanup();
			capacity = size;
			init(device, label);
		} else if (size == 0) {
			cleanup();
			capacity = size;
		} else {
			assert(buffer);
		}
	}

	size_t getCapacity() const { return capacity; }
	WGPUBufferUsageFlags getUsageFlags() const { return usage; }
	WGPUBuffer getBuffer() const { return buffer; }
	operator bool() const { return capacity > 0; }
	operator WGPUBuffer() const { return getBuffer(); }

	DynamicWGPUBuffer() = default;
	~DynamicWGPUBuffer() { cleanup(); }
	DynamicWGPUBuffer(const DynamicWGPUBuffer &other) = delete;
	DynamicWGPUBuffer &operator=(const DynamicWGPUBuffer &other) = delete;
	DynamicWGPUBuffer(DynamicWGPUBuffer &&other) : capacity(other.capacity), buffer(other.buffer), usage(other.usage) {
		other.buffer = nullptr;
		other.capacity = 0;
	}
	DynamicWGPUBuffer &operator=(DynamicWGPUBuffer &&other) {
		capacity = other.capacity;
		buffer = other.buffer;
		usage = other.usage;
		other.buffer = nullptr;
		other.capacity = 0;
		return *this;
	}

private:
	void cleanup() {
		if (buffer) {
			wgpuBufferRelease(buffer);
			buffer = nullptr;
		}
		capacity = 0;
	}

	void init(WGPUDevice device, const char *label) {
		WGPUBufferDescriptor desc{};
		desc.size = capacity;
		desc.usage = usage;
		desc.label = label;
		buffer = wgpuDeviceCreateBuffer(device, &desc);
	}
};

struct DynamicWGPUBufferPool {
	std::vector<DynamicWGPUBuffer> buffers;
	std::vector<size_t> freeList;

	void reset() {
		freeList.clear();
		for (size_t i = 0; i < buffers.size(); i++) {
			freeList.push_back(i);
		}
	}

	DynamicWGPUBuffer &allocateBufferNew() { return buffers.emplace_back(); }

	DynamicWGPUBuffer &allocateBufferAny() {
		if (freeList.size() > 0) {
			size_t bufferIndex = freeList.back();
			freeList.pop_back();
			return buffers[bufferIndex];
		} else {
			return allocateBufferNew();
		}
	}

	DynamicWGPUBuffer &allocateBuffer(size_t size) {
		int64_t smallestDelta = INT64_MAX;
		decltype(freeList)::iterator targetFreeListIt = freeList.end();
		for (auto it = freeList.begin(); it != freeList.end(); ++it) {
			size_t bufferIndex = freeList[*it];
			auto &buffer = buffers[bufferIndex];
			int64_t delta = buffer.getCapacity() - size;
			if (delta >= 0) {
				if (delta < smallestDelta) {
					smallestDelta = delta;
					targetFreeListIt = it;
				}
			}
		}

		if (targetFreeListIt != freeList.end()) {
			auto bufferIndex = *targetFreeListIt;
			freeList.erase(targetFreeListIt);
			return buffers[bufferIndex];
		} else {
			return allocateBufferNew();
		}
	}
};

} // namespace gfx
