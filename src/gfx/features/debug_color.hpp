#pragma once

#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <memory>

namespace gfx {
namespace features {

struct DebugColor {
	enum class Stage { Vertex, Fragment };
	static inline FeaturePtr create(const char *fieldName, gfx::ProgrammableGraphicsStage stage) {
		using namespace shader;
		using namespace shader::blocks;

		FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);

		FeaturePtr feature = std::make_shared<Feature>();

		auto &debugColor = feature->shaderEntryPoints.emplace_back(
			"debugColor", stage, WithInput(fieldName, WriteGlobal("color", colorFieldType, "vec4<f32>(", ReadInput(fieldName), ".xyz, 1.0)")));
		if (stage == gfx::ProgrammableGraphicsStage::Vertex) {
			debugColor.dependencies.emplace_back("initColor");
		} else {
			debugColor.dependencies.emplace_back("readColor");
		}
		debugColor.dependencies.emplace_back("writeColor", DependencyType::Before);

		return feature;
	}
};

} // namespace features
} // namespace gfx
