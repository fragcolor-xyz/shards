#pragma once

#include <vector>
#include <linalg/linalg.h>
#include <linalg_shim.hpp>
#include "mesh.hpp"
#include "chainblocks.hpp"

namespace tinygltf {
struct Animation;
}

namespace gfx {

// struct GFXSkin {
// 	GFXSkin(const tinygltf::Skin &skin, const tinygltf::Model &gltf) {
// 		if (skin.skeleton != -1) {
// 			skeleton = skin.skeleton;
// 		}

// 		joints = skin.joints;

// 		if (skin.inverseBindMatrices != -1) {
// 			auto &mats_a = gltf.accessors[skin.inverseBindMatrices];
// 			assert(mats_a.type == TINYGLTF_TYPE_MAT4);
// 			assert(mats_a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
// 			auto &view = gltf.bufferViews[mats_a.bufferView];
// 			auto &buffer = gltf.buffers[view.buffer];
// 			auto it = buffer.data.begin() + view.byteOffset + mats_a.byteOffset;
// 			const auto stride = mats_a.ByteStride(view);
// 			for (size_t i = 0; i < mats_a.count; i++) {
// 				bindPoses.emplace_back(Mat4::FromArrayUnsafe((float *)&(*it)));
// 				it += stride;
// 			}
// 		}
// 	}

// 	std::optional<int> skeleton;
// 	std::vector<int> joints;
// 	std::vector<chainblocks::Mat4> bindPoses;
// };

// using GFXSkinRef = std::reference_wrapper<GFXSkin>;

struct Node;
using NodePtr = std::shared_ptr<Node>;

struct Node {
	std::string name;
	chainblocks::Mat4 transform;

	MeshPtr mesh;
	// std::optional<GFXSkinRef> skin;

	std::vector<NodePtr> children;
};

struct Animation {
	Animation(const tinygltf::Animation &anim) {}
};

struct Model {
	std::vector<NodePtr> rootNodes;
	std::unordered_map<std::string_view, Animation> animations;

	static constexpr uint32_t CC = 'gltf';
	static inline chainblocks::Type Type{{CBType::Object, {.object = {.vendorId = CoreCC, .typeId = CC}}}};
	static inline chainblocks::Type VarType = chainblocks::Type::VariableOf(Type);
	static inline ObjectVar<Model> Var{"GLTF-Model", CoreCC, CC};
};
} // namespace gfx
