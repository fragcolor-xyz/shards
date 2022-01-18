
#pragma once

#include "context_data.hpp"
#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "linalg/linalg.h"

namespace gfx {

struct MeshVertexAttribute {
	const char *name;
	uint8_t numComponents;
	VertexAttributeType type;

	MeshVertexAttribute() = default;
	MeshVertexAttribute(const char *name, uint8_t numComponents, VertexAttributeType type = VertexAttributeType::Float32)
		: name(name), numComponents(numComponents), type(type) {}

	template <typename T> void hashStatic(T &hasher) const {
		hasher(name);
		hasher(numComponents);
		hasher(type);
	}
};

struct MeshContextData {
	WGPUBuffer vertexBuffer = nullptr;
	WGPUBuffer indexBuffer = nullptr;
};

struct Mesh : public TWithContextData<MeshContextData> {
public:
	struct Format {
		PrimitiveType primitiveType = PrimitiveType::TriangleList;
		WindingOrder windingOrder = WindingOrder::CCW;
		IndexFormat indexFormat = IndexFormat::UInt16;
		std::vector<MeshVertexAttribute> vertexAttributes;
	};

private:
	Format format;
	size_t numVertices = 0;
	size_t numIndices = 0;
	std::vector<uint8_t> vertexData;
	std::vector<uint8_t> indexData;

public:
	Mesh() {}
	~Mesh() { releaseContextDataCondtional(); }

	const Format &getFormat() const { return format; }
	void update(const Format &format, const void *inVertexData, size_t vertexSize, const void *inIndexData, size_t indexSize);

private:
	void createContextData();
	void releaseContextData();
};
using MeshPtr = std::shared_ptr<Mesh>;

} // namespace gfx
