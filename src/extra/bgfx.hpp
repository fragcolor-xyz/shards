/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#pragma once

#include "SDL.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include "bgfx/bgfx.h"
#include "bgfx/embedded_shader.h"
#include "bgfx/platform.h"
#include "gfx/context.hpp"
#include "gfx/frame.hpp"
#include "gfx/chainblocks_types.hpp"
#include "blocks/shared.hpp"
#include "chainblocks.hpp"
#include "linalg_shim.hpp"

using namespace chainblocks;
namespace BGFX {

// FROM BGFX, MIGHT BREAK IF BGFX CHANGES
constexpr bool isShaderVerLess(uint32_t _magic, uint8_t _version) {
	return (_magic & BX_MAKEFOURCC(0, 0, 0, 0xff)) < BX_MAKEFOURCC(0, 0, 0, _version);
}
// ~ FROM BGFX, MIGHT BREAK IF BGFX CHANGES

constexpr uint32_t getShaderOutputHash(const bgfx::Memory *mem) {
	uint32_t magic = *(uint32_t *)mem->data;
	uint32_t hashIn = *(uint32_t *)(mem->data + 4);
	uint32_t hashOut = 0x0;
	if (isShaderVerLess(magic, 6)) {
		hashOut = hashIn;
	}
	else {
		hashOut = *(uint32_t *)(mem->data + 8);
	}
	return hashOut;
}

inline void overrideShaderInputHash(const bgfx::Memory *mem, uint32_t hash) {
	// hack/fix hash in or creation of program will fail!
	// otherwise bgfx would fail inside createShader
	*(uint32_t *)(mem->data + 4) = hash;
}

inline const bgfx::EmbeddedShader::Data &findEmbeddedShader(const bgfx::EmbeddedShader &shader) {
	bgfx::RendererType::Enum type = bgfx::getRendererType();
	for (uint32_t i = 0; i < bgfx::RendererType::Count; ++i) {
		if (shader.data[i].type == type) {
			return shader.data[i];
		}
	}
	throw std::runtime_error("Could not find embedded shader");
}

struct Base {
	CBVar *_bgfxCtx{nullptr};
};

struct BaseConsumer : public Base {
	static inline CBExposedTypeInfo ContextInfo = ExposedInfo::Variable("GFX.Context", CBCCSTR("The graphics context."), gfx::ContextType);
	static inline ExposedInfo requiredInfo = ExposedInfo(ContextInfo);

	CBExposedTypesInfo requiredVariables() { return CBExposedTypesInfo(requiredInfo); }

	// Required before _bgfxCtx can be used
	void _warmup(CBContext *context) {
		_bgfxCtx = referenceVariable(context, "GFX.Context");
		assert(_bgfxCtx->valueType == CBType::Object);
	}

	// Required during cleanup if _warmup() was called
	void _cleanup() {
		if (_bgfxCtx) {
			releaseVariable(_bgfxCtx);
			_bgfxCtx = nullptr;
		}
	}

	CBTypeInfo compose(const CBInstanceData &data) {
		if (data.onWorkerThread) {
			throw chainblocks::ComposeError(
				"GFX Blocks cannot be used on a worker thread (e.g. "
				"within an Await block)");
		}
		return CoreInfo::NoneType; // on purpose to trigger assertion during
								   // validation
	}

	gfx::Context &getGfxContext() {
		gfx::Context *contextPtr = reinterpret_cast<gfx::Context *>(_bgfxCtx->payload.objectValue);
		if (!contextPtr) {
			throw chainblocks::ActivationError("Graphics context not set");
		}
		return *contextPtr;
	}

	gfx::FrameRenderer &getFrameRenderer() {
		gfx::FrameRenderer *frameRendererPtr = getGfxContext().getFrameRenderer();
		if (!frameRendererPtr) {
			throw chainblocks::ActivationError("No frame is being rendered");
		}
		return *frameRendererPtr;
	}

	const gfx::FrameRenderer &getFrameRenderer() const { return const_cast<BaseConsumer *>(this)->getFrameRenderer(); }

	const gfx::FrameInputs &getFrameInputs() const { return getFrameRenderer().inputs; }

	SDL_Window *getSDLWindow() { return getGfxContext().window; }

	const std::vector<SDL_Event> &getSDLEvents() const { return getFrameInputs().events; }
};
}; // namespace BGFX

namespace chainblocks {
struct IShaderCompiler {
	virtual ~IShaderCompiler() {}

	virtual CBVar compile(
		std::string_view varyings, //
		std::string_view code,	   //
		std::string_view type,	   //
		std::string_view defines,  //
		CBContext *context		   //
		) = 0;

	virtual void warmup(CBContext *context) = 0;
};
extern std::unique_ptr<IShaderCompiler> makeShaderCompiler();
} // namespace chainblocks
