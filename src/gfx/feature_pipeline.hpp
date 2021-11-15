#pragma once

#include "bgfx/bgfx.h"
#include "drawable.hpp"
#include "enums.hpp"
#include "feature.hpp"
#include "fields.hpp"
#include "mesh.hpp"
#include <bgfx/bgfx.h>
#include <stdint.h>
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
	bgfx::VertexLayout vertexLayout;

	FeaturePipeline() = default;
	FeaturePipeline(const FeaturePipeline &) = delete;
	FeaturePipeline &operator==(const FeaturePipeline &) = delete;
	~FeaturePipeline();
};
typedef std::shared_ptr<FeaturePipeline> FeaturePipelinePtr;

struct PipelineOutputDesc {
	std::string name;
	bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8;
};

struct FeatureBindingLayout;
struct BuildPipelineParams {
	const FeatureBindingLayout &bindingLayout;
	RendererType rendererType = RendererType::None;
	struct IShaderCompiler *shaderCompiler = nullptr;
	WindingOrder faceWindingOrder = WindingOrder::CCW;
	std::vector<MeshVertexAttribute> vertexAttributes;
	std::vector<const Feature *> features;
	std::vector<PipelineOutputDesc> outputs;
};

struct BuildBindingLayoutParams {
	const std::vector<const Feature *> &features;
	const Drawable &drawable;
};

struct FeatureBinding {
	std::string name;
	bgfx::UniformHandle handle;
	FieldType type;
};

struct FeatureBasicBinding : public FeatureBinding {
	FieldVariant defaultValue;
};

struct FeatureTextureBinding : public FeatureBinding {
	TexturePtr defaultValue;
};

struct FeatureBindingLayout {
	std::vector<FeatureBasicBinding> basicBindings;
	std::vector<FeatureTextureBinding> textureBindings;

	FeatureBindingLayout() = default;
	FeatureBindingLayout(const FeatureBindingLayout &) = delete;
	FeatureBindingLayout &operator==(const FeatureBindingLayout &) = delete;
	~FeatureBindingLayout();
};
typedef std::shared_ptr<FeatureBindingLayout> FeatureBindingLayoutPtr;

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
