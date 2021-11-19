
#pragma once

#include "bgfx/bgfx.h"
#include "linalg/linalg.h"
#include "material.hpp"
#include "tinygltf/tiny_gltf.h"

namespace gfx {

enum class MeshPrimitiveType { TriangleList, TriangleStrip };

struct MeshPipelineParameters {
	MeshPrimitiveType primitiveType = MeshPrimitiveType::TriangleList;
};

struct Primitive {
	bgfx::VertexBufferHandle vb = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle ib = BGFX_INVALID_HANDLE;
	bgfx::VertexLayout vertexLayout{};

	// StaticUsageParameters staticUsageParameters;
	// MeshPipelineParameters pipelineParameters;

	std::shared_ptr<Material> material;

	Primitive() {}

	Primitive(Primitive &&other) {
		std::swap(vb, other.vb);
		std::swap(ib, other.ib);
		std::swap(vertexLayout, other.vertexLayout);
		std::swap(material, other.material);
		// std::swap(staticUsageParameters, other.staticUsageParameters);
		// std::swap(pipelineParameters, other.pipelineParameters);
	}

	~Primitive() {
		if (bgfx::isValid(vb)) {
			bgfx::destroy(vb);
		}
		if (bgfx::isValid(ib)) {
			bgfx::destroy(ib);
		}
	}
};

struct Mesh {
	std::string name;
	std::vector<Primitive> primitives;
};
using MeshPtr = std::shared_ptr<Mesh>;

} // namespace gfx
