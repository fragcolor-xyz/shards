#pragma once

#include "bgfx/bgfx.h"
#include "drawable.hpp"
#include "feature.hpp"
#include "fields.hpp"
#include <bgfx/bgfx.h>
#include <cstdint>
#include <memory>

namespace gfx {

struct Feature;
struct Drawable;

struct TextureBinding {
	size_t slot;
	std::shared_ptr<Texture> defaultValue;
};

struct Binding {
	bgfx::UniformHandle handle;
	std::string name;
	std::optional<TextureBinding> texture;
	std::vector<uint8_t> defaultValue;
};

struct ShaderStageCompilerInputs {
	std::string code;
};

struct ShaderCompilerInputs {
	std::string varyings;
	ShaderStageCompilerInputs stages[2];
};

struct FeaturePipeline {
	BGFXPipelineState state;
	bgfx::ProgramHandle program;
	std::vector<Binding> bindings;
	bgfx::VertexLayout vertexLayout;

	~FeaturePipeline();
};
typedef std::shared_ptr<FeaturePipeline> FeaturePipelinePtr;

struct PipelineOutputDesc {
	std::string name;
	bgfx::TextureFormat::Enum format;
};

struct BuildPipelineParams {
	RendererType rendererType = RendererType::None;
	struct IShaderCompiler *shaderCompiler = nullptr;
	const Drawable *drawable = nullptr;
	std::vector<const Feature *> features;
	std::vector<PipelineOutputDesc> outputs;
};

struct BuildBindingLayoutParams {
	const std::vector<const Feature *>& features;
	const Drawable& drawable;
};

struct FeatureBasicBinding {
	std::string name;
	bgfx::UniformHandle handle;
};

struct FeatureTextureBinding {
	std::string name;
	bgfx::UniformHandle handle;
};

struct FeatureBindingLayout {
	std::vector<FeatureBasicBinding> basicBindings;
	std::vector<FeatureTextureBinding> textureBindings;

	~FeatureBindingLayout();
};
typedef std::shared_ptr<FeatureBindingLayout> FeatureBindingLayoutPtr;

struct FeatureBindingValues {
	std::vector<std::vector<uint8_t>> basicBindings;
	std::vector<TexturePtr> textureBindings;
	std::unordered_map<std::string, size_t> textureMap;
};

template <typename T> std::vector<const Feature *> featuresToPointers(T iterable) {
	std::vector<const Feature *> result;
	for (auto &featurePtr : iterable) {
		result.push_back(featurePtr.get());
	}
	return result;
}

bool buildBindingLayout(const BuildBindingLayoutParams &params, FeatureBindingLayout &outBindingLayout);
bool buildPipeline(const BuildPipelineParams &params, FeaturePipeline &outPipeline, ShaderCompilerInputs *outShaderCompilerInputs = nullptr);

} // namespace gfx
