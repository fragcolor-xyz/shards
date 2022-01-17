
#pragma once

#include "enums.hpp"
#include "linalg/linalg.h"
#include "tinygltf/tiny_gltf.h"

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

struct Mesh {
	std::vector<MeshVertexAttribute> vertexAttributes;
	PrimitiveType primitiveType = PrimitiveType::TriangleList;
	WindingOrder windingOrder = WindingOrder::CCW;

	Mesh() {}
	~Mesh() {}
};
using MeshPtr = std::shared_ptr<Mesh>;

} // namespace gfx
