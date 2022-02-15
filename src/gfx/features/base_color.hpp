#pragma once

#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <memory>

namespace gfx {
namespace features {

struct BaseColor {
	static inline FeaturePtr create() {
		using namespace shader;
		using namespace shader::blocks;

		FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);

		FeaturePtr feature = std::make_shared<Feature>();
		feature->shaderParams.emplace_back("baseColor", float4(1, 1, 1, 1));
		// feature->

		const char *defaultColor = "vec4<f32>(1.0, 1.0, 1.0, 1.0)";

		auto readColorParam = makeCompoundBlock(ReadBuffer("object"), ".baseColor");

		feature->shaderEntryPoints.emplace_back(
			"initColor", ProgrammableGraphicsStage::Vertex,
			WriteGlobal("color", colorFieldType, WithInput("color", ReadInput("color"), defaultColor), "*", std::move(readColorParam)));
		auto &writeColor =
			feature->shaderEntryPoints.emplace_back("writeColor", ProgrammableGraphicsStage::Vertex, WriteOutput("color", colorFieldType, ReadGlobal("color")));
		writeColor.dependencies.emplace_back("initColor");

		EntryPoint &applyVertexColor = feature->shaderEntryPoints.emplace_back(
			"applyVertexColor", ProgrammableGraphicsStage::Vertex,
			WithInput("color", WriteGlobal("color", colorFieldType, makeCompoundBlock(ReadGlobal("color"), "*", ReadInput("color"), ";"))));
		applyVertexColor.dependencies.emplace_back("initColor", DependencyType::After);
		applyVertexColor.dependencies.emplace_back("writeColor", DependencyType::Before);

		feature->shaderEntryPoints.emplace_back("readColor", ProgrammableGraphicsStage::Fragment,
												WriteGlobal("color", colorFieldType, WithInput("color", ReadInput("color"), defaultColor)));
		feature->shaderEntryPoints.emplace_back("color", ProgrammableGraphicsStage::Fragment,
												WithOutput("color", WriteOutput("color", colorFieldType, ReadGlobal("color"))));

		return feature;
	}
};

} // namespace features
} // namespace gfx
