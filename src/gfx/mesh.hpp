
#pragma once

#include "bgfx/bgfx.h"
#include "enums.hpp"
#include "linalg/linalg.h"
#include "material.hpp"
#include "tinygltf/tiny_gltf.h"

namespace gfx {

struct MeshVertexAttribute {
	bgfx::Attrib::Enum tag;
	uint8_t numComponents;
	bgfx::AttribType::Enum type;
	bool normalized = false;
	bool asInt = false;
};

struct Mesh {
	bgfx::VertexBufferHandle vb = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle ib = BGFX_INVALID_HANDLE;
	std::vector<MeshVertexAttribute> vertexAttributes;
	PrimitiveType primitiveType = PrimitiveType::TriangleList;
	WindingOrder windingOrder = WindingOrder::CCW;

	Mesh() {}
	~Mesh() {
		if (bgfx::isValid(vb)) {
			bgfx::destroy(vb);
		}
		if (bgfx::isValid(ib)) {
			bgfx::destroy(ib);
		}
	}
};
using MeshPtr = std::shared_ptr<Mesh>;

} // namespace gfx
