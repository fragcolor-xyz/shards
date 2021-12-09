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

struct Pipeline {
	BGFXPipelineState state;
	std::vector<Binding> bindings;
	bgfx::ProgramHandle program;
	bgfx::VertexLayout vertexLayout;
};

struct PipelineOutputDesc {
	std::string name;
	bgfx::TextureFormat format;
};

struct BuildPipelineParams {
	bgfx::RendererType::Enum rendererType;
	const Drawable *drawable = nullptr;
	std::vector<const Feature *> features;
	std::vector<PipelineOutputDesc> outputs;
};

bool buildPipeline(const BuildPipelineParams &params, Pipeline &outPipeline);

} // namespace gfx
