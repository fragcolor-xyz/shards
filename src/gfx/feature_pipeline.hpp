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

struct Pipeline {
	BGFXPipelineState state;
	std::vector<Binding> bindings;
	bgfx::ProgramHandle program;
	bgfx::VertexLayout vertexLayout;
	std::optional<ShaderCompilerInputs> debug;
};

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

template <typename T> std::vector<const Feature *> featuresToPointers(T iterable) {
	std::vector<const Feature *> result;
	for (auto &featurePtr : iterable) {
		result.push_back(featurePtr.get());
	}
	return result;
}
bool buildPipeline(const BuildPipelineParams &params, Pipeline &outPipeline);

} // namespace gfx
