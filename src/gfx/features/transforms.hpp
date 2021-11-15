#pragma once

#include "../enums.hpp"
#include "../feature.hpp"
#include "../fields.hpp"
#include <memory>

namespace gfx {
namespace features {

struct Transform {
	static inline FeaturePtr create() {
		FeaturePtr feature = std::make_shared<Feature>();
		feature->shaderVaryings.emplace_back("normal", FieldType::Float3);
		feature->shaderVaryings.emplace_back("tangent", FieldType::Float3);
		feature->shaderVaryings.emplace_back("worldPos", FieldType::Float3);

		auto &vertCode = feature->shaderCode.emplace_back("transform", ProgrammableGraphicsStage::Vertex);
		vertCode.code =
			R"(
void transformNormal(vec3 localNormal, out vec3 outWorldNormal) {
	outWorldNormal = mul(u_model[0], vec4(localNormal, 0.0)).xyz;
}

void main(inout MaterialInfo mi) {
	vec4 hpos = mul(u_model[0], vec4(mi.in_position, 1.0));
	mi.worldPos = hpos.xyz;
	mi.out_position = mul(u_viewProj, hpos);
#ifdef VERTEX_NORMAL_COMPONENTS
	transformNormal(mi.in_normal.xyz, mi.normal.xyz);
#endif
#ifdef VERTEX_TANGENT_COMPONENTS
	transformNormal(mi.in_tangent.xyz, mi.tangent.xyz);
#endif
})";
		return feature;
	}
};

} // namespace features
} // namespace gfx
