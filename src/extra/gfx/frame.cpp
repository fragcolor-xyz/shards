#include "frame.hpp"
#include "context.hpp"
#include "linalg_shim.hpp"
#include "mesh.hpp"
#include <bgfx/bgfx.h>
#include "chainblocks.hpp"
#include "foundation.hpp"

namespace gfx {
using Vec4 = chainblocks::Vec4;
using Vec3 = chainblocks::Vec3;
using Mat4 = chainblocks::Mat4;

void FrameRenderer::begin(CBContext &cbContext) {
	context.beginFrame(this);
	this->cbContext = &cbContext;

	float time[4] = {inputs.deltaTime, inputs.time, float(inputs.frameNumber), 0.0};
	bgfx::setUniform(context._timeUniformHandle, time, 1);
}

void FrameRenderer::end(CBContext &cbContext) {
	assert(_viewsStack.size() == 0);
	
	context.endFrame(this);
	this->cbContext = nullptr;

	bgfx::frame();
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

ViewInfo &FrameRenderer::getCurrentView() {
	assert(_viewsStack.size() > 0);
	return _viewsStack.back();
}

ViewInfo &FrameRenderer::pushView(ViewInfo view) {
	bgfx::touch(view.id);
	bgfx::setViewRect(view.id, view.viewport.x, view.viewport.y, view.viewport.width, view.viewport.height);

	return _viewsStack.emplace_back(view);
}

ViewInfo &FrameRenderer::pushMainView() {
	ViewInfo view;
	view.viewport = Rect(context.width, context.height);
	return pushView(view);
}

void FrameRenderer::popView() {
	assert(_viewsStack.size() > 0);
	_viewsStack.pop_back();
}

bgfx::ViewId FrameRenderer::nextViewId() {
	assert(_nextViewCounter < UINT16_MAX);
	return _nextViewCounter++;
}

void FrameRenderer::draw(const Primitive &primitive) {
	// HACK: for shader reload
	for (const SDL_Event &event : inputs.events) {
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_F5) {
				CBLOG_INFO("Clearing shader cache");
				context.shaderCache.clear();
				break;
			}
		}
	}

	if (!primitive.material) return;
	Material &material = const_cast<Material &>(*primitive.material.get());
	StaticUsageParameters usage = primitive.staticUsageParameters;

	// TODO: Modify usage.flags here based on instancing/picking

	const StaticMaterialParameters &staticMaterialParameters = material.getStaticMaterialParameters();
	const DynamicMaterialParameters &dynamicMaterialParameters = material.dynamicMaterialParameters;
	const MeshPipelineParameters &meshPipelineParameters = primitive.pipelineParameters;

	ShaderProgramPtr shaderProgram = context.shaderCache.getOrCreateProgram(*cbContext, *context.shaderCompiler.get(), primitive.material, usage);
	if (!shaderProgram) return;

	bool isRenderingFlipped = false; // TODO: Set this
	uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
	if (!dynamicMaterialParameters.doubleSided) {
		if (isRenderingFlipped) {
			state |= BGFX_STATE_CULL_CCW;
		}
		else {
			state |= BGFX_STATE_CULL_CW;
		}
	}

	if (meshPipelineParameters.primitiveType == MeshPrimitiveType::TriangleStrip) {
		state |= BGFX_STATE_PT_TRISTRIP;
	}

	bgfx::setState(state);

	bgfx::setVertexBuffer(0, primitive.vb);
	if (bgfx::isValid(primitive.ib)) {
		bgfx::setIndexBuffer(primitive.ib);
	}

	size_t samplerIndex = 0;
	{
		auto Binding_Float = [&](bgfx::UniformHandle uniform, const float &v) {
			Vec4 v4(v, 0.0f, 0.0f, 0.0f);
			bgfx::setUniform(uniform, &v4);
		};
		auto Binding_Float3 = [&](bgfx::UniformHandle uniform, const linalg::vec<float, 3> &v) {
			Vec4 v4(v.x, v.y, v.z, 0.0f);
			bgfx::setUniform(uniform, &v4);
		};
		auto Binding_Float4 = [&](bgfx::UniformHandle uniform, const linalg::vec<float, 4> &v4) { bgfx::setUniform(uniform, &v4); };
		auto Binding_Texture2D = [&](bgfx::UniformHandle uniform, const TexturePtr &v) {
			if (v) {
				bgfx::setTexture(samplerIndex++, uniform, v->handle);
			}
		};

		const ShaderBindingPointUniforms &uniforms = *context.bindingUniforms.get();

#define Binding(_name, _type) _type(uniforms._name, material.dynamicMaterialParameters._name);
#include "shaders/bindings.def"
#undef Binding
	}

	bgfx::submit(getCurrentView().id, shaderProgram->program);
}

FrameRenderer *FrameRenderer::get(Context &context) {
	return context.getFrameRenderer();
}
} // namespace gfx
