#include "feature_pipeline.hpp"
#include "bgfx/bgfx.h"
#include "drawable.hpp"
#include "enums.hpp"
#include "feature.hpp"
#include "fields.hpp"
#include "mesh.hpp"
#include "texture.hpp"
#include <unordered_map>
#include <variant>

namespace gfx {

struct VertexAttribute {
	bgfx::AttribType type;
};

struct ShaderStageBuilder {
	std::vector<FeatureShaderCode> codeSegments;
};

struct ShaderBuilder {
	const std::vector<MeshVertexAttribute> &vertexAttributes;
	ShaderStageBuilder vertex;
	ShaderStageBuilder fragment;
	std::vector<FeatureShaderField> globals;
	std::vector<FeatureShaderField> params;
	std::vector<FeatureShaderField> varyings;
};

bool buildPipeline(const BuildPipelineParams &params, Pipeline &outPipeline) {
	const Mesh &mesh = *params.drawable->mesh.get();
	const Material &material = *params.drawable->material.get();

	std::unordered_map<std::string, size_t> textureMap;
	ShaderBuilder shaderBuilder{mesh.vertexAttributes};
	FeaturePipelineState state;

	bgfx::VertexLayout vertexLayout;
	vertexLayout.begin(params.rendererType);
	for (auto &attribute : mesh.vertexAttributes) {
		vertexLayout.add(attribute.tag, attribute.numComponents, attribute.type, attribute.normalized, attribute.asInt);
	}
	vertexLayout.end();

	for (auto &featurePtr : params.features) {
		const Feature &feature = *featurePtr;
		state = state.combine(feature.state);

		for (auto &shaderParam : feature.shaderParams) {
			Binding &binding = outPipeline.bindings.emplace_back();
			binding.name = shaderParam.name;
			if (shaderParam.type == FieldType::Texture) {
				binding.texture = TextureBinding{textureMap.size()};
				if (const TexturePtr *texture = std::get_if<TexturePtr>(&shaderParam.defaultValue)) {
					binding.texture->defaultValue = *texture;
				}
			} else {
				packFieldVariant(shaderParam.defaultValue, binding.defaultValue);
			}
		}

		for (auto &shaderCode : feature.shaderCode) {
			if (shaderCode.stage == ProgrammableGraphicsStage::Vertex)
				shaderBuilder.vertex.codeSegments.push_back(shaderCode);
			else
				shaderBuilder.fragment.codeSegments.push_back(shaderCode);
		}

		for (auto &param : feature.shaderParams) {
			shaderBuilder.params.push_back(param);
		}

		for (auto &varying : feature.shaderVaryings) {
			shaderBuilder.varyings.push_back(varying);
		}
	}

	// {Feature.sh};

	// TODO
	// outPipeline.program;

	state.toBGFXState(mesh.windingOrder);
}

} // namespace gfx
