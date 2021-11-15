#pragma once

#include "bgfx/bgfx.h"
#include "enums.hpp"
#include "types.hpp"
#include <SDL_events.h>
#include <SDL_video.h>
#include <cassert>
#include <memory>
#include <stdexcept>

namespace gfx {
struct BGFXException : public std::runtime_error {
	uint16_t line;
	bgfx::Fatal::Enum code;
	BGFXException(const char *filepath, uint16_t line, bgfx::Fatal::Enum code, const char *str);
};

struct IDrawable {
	// virtual CBChain *getChain() = 0;
};

struct ContextCreationOptions {
	bool debug = false;
	bgfx::TextureFormat::Enum backbufferFormat = bgfx::TextureFormat::RGBA8;
	RendererType renderer = RendererType::None;
	void *overrideNativeWindowHandle = nullptr;

	ContextCreationOptions();
};

struct ImguiContext;
struct Primitive;
struct MaterialBuilderContext;
struct BGFXCallbacks;
struct FrameRenderer;
struct Window;
class ShaderCompilerModule;
struct ICapture;
struct Context {
public:
	std::shared_ptr<BGFXCallbacks> bgfxCallbacks;

	uint32_t resetFlags = BGFX_RESET_VSYNC;

	bgfx::UniformHandle timeUniformHandle = BGFX_INVALID_HANDLE;

private:
	RendererType rendererType = RendererType::None;
	FrameRenderer *currentFrameRenderer = nullptr;
	Window *window;
	int2 mainOutputSize;
	std::vector<bgfx::UniformHandle> samplers;

public:
	Context();
	~Context();
	void init(Window &window, const ContextCreationOptions &options = ContextCreationOptions{});
	void cleanup();
	bool isInitialized() const { return rendererType != RendererType::None; }

	Window &getWindow() {
		assert(window);
		return *window;
	}

	const bgfx::UniformHandle &getSampler(size_t index);

	RendererType getRendererType() const { return rendererType; }

	int2 getMainOutputSize() const { return mainOutputSize; }
	void resizeMainOutput(const int2 &newSize);
	void resizeMainOutputConditional(const int2 &newSize);

	void addCapture(std::weak_ptr<ICapture> capture);
	void removeCapture(std::weak_ptr<ICapture> capture);

	// used before and after frame to synchronize capture callbacks
	void captureMark();
	void captureSync(uint32_t timeout = -1);

	void beginFrame(FrameRenderer *frameRenderer);
	void endFrame(FrameRenderer *frameRenderer);
	FrameRenderer *getFrameRenderer() { return currentFrameRenderer; }
};

struct FrameCaptureSync {
	Context &context;
	FrameCaptureSync(Context &context) : context(context) { context.captureMark(); }
	~FrameCaptureSync() { context.captureSync(); }
};

} // namespace gfx
