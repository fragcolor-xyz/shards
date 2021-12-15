
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

	MeshVertexAttribute() = default;
	MeshVertexAttribute(bgfx::Attrib::Enum tag, uint8_t numComponents, bgfx::AttribType::Enum type = bgfx::AttribType::Float, bool normalized = false,
						bool asInt = false)
		: tag(tag), numComponents(numComponents), type(type), normalized(normalized), asInt(asInt) {}

	template <typename T> void hashStatic(T &hasher) const {
		hasher(tag);
		hasher(numComponents);
		hasher(type);
		hasher(normalized);
		hasher(asInt);
	}
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

template <typename T> bgfx::VertexLayout createVertexLayout(T iterable, bgfx::RendererType::Enum rendererType = bgfx::RendererType::Noop) {
	bgfx::VertexLayout result;
	result.begin(rendererType);
	for (const MeshVertexAttribute &attribute : iterable) {
		result.add(attribute.tag, attribute.numComponents, attribute.type, attribute.normalized, attribute.asInt);
	}
	result.end();
	return result;
}

} // namespace gfx
