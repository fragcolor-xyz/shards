#pragma once

#include "../feature.hpp"
#include "../enums.hpp"
#include "../fields.hpp"
#include <memory>

namespace gfx {
namespace features {

struct BaseColor {
	static inline FeaturePtr create() {
		FeaturePtr feature = std::make_shared<Feature>();
		feature->shaderParams.emplace_back("baseColor", float4(1, 1, 1, 1));
		feature->shaderVaryings.emplace_back("color", float4(1, 1, 1, 1));

		auto &param = feature->shaderParams.emplace_back("baseColorTexture", FieldType::Texture2D);
		param.flags = FeatureShaderFieldFlags::Optional;

		auto &vertColorCode = feature->shaderCode.emplace_back("copyVertexColor", ProgrammableGraphicsStage::Vertex);
		vertColorCode.code =
			R"(void main(inout MaterialInfo mi) {
#ifdef VERTEX_COLOR_COMPONENTS
	mi.color = mi.in_color;
#endif
})";

		auto &code0 = feature->shaderCode.emplace_back("setupColor");
		code0.code =
			R"(void main(inout MaterialInfo mi) {
	// varying color multiplied by uniform value
	mi.color *= u_baseColor;
#if HAS_baseColorTexture
	mi.color *= texture2D(u_baseColorTexture, mi.texcoord0);
#endif
})";

		auto &code1 = feature->shaderCode.emplace_back("outputColor");
		code1.code =
			R"(void main(inout MaterialInfo mi) {
#ifdef OUTPUT_color
	mi.out_color = mi.color;
#endif
})";
		code1.dependencies.emplace_back("setupColor", DependencyType::After);
		return feature;
	}
};

} // namespace features
} // namespace gfx
