#include "frame.hpp"
#include "context.hpp"
#include "mesh.hpp"
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

namespace gfx {
void FrameRenderer::begin() {
	context.beginFrame(this);

	float time[4] = {inputs.deltaTime, inputs.time, float(inputs.frameNumber),
					 0.0};
	bgfx::setUniform(context.timeUniformHandle, time, 1);
}

uint32_t FrameRenderer::end(bool capture) {
	assert(views.size() == 0);
	context.endFrame(this);
	return bgfx::frame(capture);
}

uint32_t FrameRenderer::addFrameDrawable(IDrawable *drawable) {
	// 0 idx = empty
	uint32_t id = ++_frameDrawablesCount;
	_frameDrawables[id] = drawable;
	return id;
}

IDrawable *FrameRenderer::getFrameDrawable(uint32_t id) {
	if (id == 0) {
		return nullptr;
	}
	return _frameDrawables[id];
}

View &FrameRenderer::getCurrentView() {
	assert(views.size() > 0);
	return *views.back().get();
}

View &FrameRenderer::pushView() {
	View &view =
		*views.emplace_back(std::make_shared<View>(nextViewId())).get();
	bgfx::touch(view.id);

	return view;
}

View &FrameRenderer::pushMainOutputView() {
	int2 mainOutputSize = context.getMainOutputSize();
	View &view = pushView();

	Rect viewport(mainOutputSize.x, mainOutputSize.y);
	view.setViewport(viewport);

	return view;
}

void FrameRenderer::popView() {
	assert(views.size() > 0);
	views.pop_back();
}

bgfx::ViewId FrameRenderer::nextViewId() {
	assert(_nextViewCounter < UINT16_MAX);
	return _nextViewCounter++;
}

// void FrameRenderer::draw(const Primitive &primitive) {
// 	// HACK: for shader reload
// 	for (const SDL_Event &event : inputs.events) {
// 		if (event.type == SDL_KEYDOWN) {
// 			if (event.key.keysym.sym == SDLK_F5) {
// 				spdlog::debug("Clearing shader cache");
// 				// context.shaderCache.clear();
// 				break;
// 			}
// 		}
// 	}

// 	if (!primitive.material)
// 		return;
// 	Material &material = const_cast<Material &>(*primitive.material.get());
// 	StaticUsageParameters usage = primitive.staticUsageParameters;

// 	// TODO: Modify usage.flags here based on instancing/picking

// 	const StaticMaterialParameters &staticMaterialParameters =
// 		material.getStaticMaterialParameters();
// 	const DynamicMaterialParameters &dynamicMaterialParameters =
// 		material.dynamicMaterialParameters;
// 	const MeshPipelineParameters &meshPipelineParameters =
// 		primitive.pipelineParameters;

// 	assert(false); // TODO
// 	ShaderProgramPtr
// 		shaderProgram; // context.shaderCache.getOrCreateProgram(*context.shaderCompiler.get(),
// 					   // primitive.material, usage);
// 	if (!shaderProgram)
// 		return;

// 	bool isRenderingFlipped = false; // TODO: Set this
// 	uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
// 					 BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
// 	if (!dynamicMaterialParameters.doubleSided) {
// 		if (isRenderingFlipped) {
// 			state |= BGFX_STATE_CULL_CCW;
// 		} else {
// 			state |= BGFX_STATE_CULL_CW;
// 		}
// 	}

// 	if (meshPipelineParameters.primitiveType ==
// 		MeshPrimitiveType::TriangleStrip) {
// 		state |= BGFX_STATE_PT_TRISTRIP;
// 	}

// 	bgfx::setState(state);

// 	bgfx::setVertexBuffer(0, primitive.vb);
// 	if (bgfx::isValid(primitive.ib)) {
// 		bgfx::setIndexBuffer(primitive.ib);
// 	}

// 	size_t samplerIndex = 0;
// 	{
// 		auto Binding_Float = [&](bgfx::UniformHandle uniform, const float &v) {
// 			float4 v4(v, 0.0f, 0.0f, 0.0f);
// 			bgfx::setUniform(uniform, &v4);
// 		};
// 		auto Binding_Float3 = [&](bgfx::UniformHandle uniform,
// 								  const float3 &v) {
// 			float4 v4(v.x, v.y, v.z, 0.0f);
// 			bgfx::setUniform(uniform, &v4);
// 		};
// 		auto Binding_Float4 = [&](bgfx::UniformHandle uniform,
// 								  const float4 &v4) {
// 			bgfx::setUniform(uniform, &v4);
// 		};
// 		auto Binding_Texture2D = [&](bgfx::UniformHandle uniform,
// 									 const TexturePtr &v) {
// 			if (v) {
// 				bgfx::setTexture(samplerIndex++, uniform, v->handle);
// 			}
// 		};

// 		const ShaderBindingPointUniforms &uniforms =
// 			*context.bindingUniforms.get();

// #define Binding(_name, _type)                                                  \
// 	_type(uniforms._name, material.dynamicMaterialParameters._name);
// #include "shaders/bindings.def"
// #undef Binding
// 	}

// 	bgfx::submit(getCurrentView().id, shaderProgram->program);
// }

FrameRenderer *FrameRenderer::get(Context &context) {
	return context.getFrameRenderer();
}
} // namespace gfx
