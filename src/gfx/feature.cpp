#include "feature.hpp"
#include "bgfx/defines.h"
#include "fields.hpp"
#include <bgfx/defines.h>

namespace gfx {

FeatureShaderField::FeatureShaderField(std::string name, FieldType type, FieldVariant defaultValue) : type(type), name(name), defaultValue(defaultValue) {}
FeatureShaderField::FeatureShaderField(std::string name, FieldVariant defaultValue)
	: type(getFieldVariantType(defaultValue)), name(name), defaultValue(defaultValue) {}

BGFXPipelineState FeaturePipelineState::toBGFXState(WindingOrder frontFace) const {
	BGFXPipelineState result;
	uint64_t &state = result.state;

	if (depthTest.has_value()) {
		state |= depthTest.value() & BGFX_STATE_DEPTH_TEST_MASK;
	} else {
		state |= BGFX_STATE_DEPTH_TEST_LESS;
	}

	if (depthWrite.has_value()) {
		state |= depthWrite.value() & BGFX_STATE_WRITE_Z;
	} else {
		state |= BGFX_STATE_WRITE_Z;
	}

	if (stencilTest.has_value() || stencilOp.has_value()) {
		result.stencil = BGFXStencilState{};
	}

	if (stencilTest.has_value()) {
		result.stencil->front |= stencilTest.value() & (BGFX_STENCIL_MASK);
	}

	if (stencilOp.has_value()) {
		result.stencil->front |= stencilOp.value() & (BGFX_STENCIL_MASK);
	}

	if (colorWrite.has_value()) {
		state |= colorWrite.value() & (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	} else {
		state |= BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
	}

	if (blend.has_value()) {
		state |= blend.value() & (BGFX_STATE_BLEND_MASK | BGFX_STATE_BLEND_EQUATION_MASK);
	}

	if (conservativeRaster.value_or(false)) {
		state |= BGFX_STATE_CONSERVATIVE_RASTER;
	}

	if (frontFace == WindingOrder::CCW) {
		state |= BGFX_STATE_FRONT_CCW;
	}

	if (this->culling.value_or(true)) {
		uint64_t cullFace = frontFace == WindingOrder::CW ? 0 : 1; // CW / CCW
		if (this->flipFrontFace.value_or(false)) {
			cullFace = (cullFace + 1) % 2;
		}
		state |= cullFace << BGFX_STATE_CULL_SHIFT;
	}

	if (scissor.has_value()) {
		result.scissor = scissor;
	}

	return result;
}

void test() {}

} // namespace gfx
