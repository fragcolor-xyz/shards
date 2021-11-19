#pragma once

#include "bgfx/bgfx.h"
// #include "shader_cache.hpp"
#include "types.hpp"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>
#include <bgfx/embedded_shader.h>
#include <cassert>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace gfx {
struct BGFXException : public std::runtime_error {
	uint16_t line;
	bgfx::Fatal::Enum code;
	BGFXException(const char *filepath, uint16_t line, bgfx::Fatal::Enum code, const char *str);
};

constexpr uint16_t PickingBufferSize = 128;
constexpr uint32_t MaxNumLights = 4;

// BGFX_CONFIG_MAX_VIEWS is 256
constexpr bgfx::ViewId MaxViews = 256;
constexpr bgfx::ViewId GuiViewId = MaxViews - 1;
constexpr bgfx::ViewId BlittingViewId = GuiViewId - 1;
constexpr bgfx::ViewId PickingViewId = BlittingViewId - 1;

struct IDrawable {
	// virtual CBChain *getChain() = 0;
};

enum class RendererType : uint8_t { None, DirectX11, Vulkan, OpenGL, Metal };
std::string_view getRendererTypeName(RendererType renderer);

struct ShaderBindingPointUniforms {
#define Binding_Float bgfx::UniformType::Vec4
#define Binding_Float3 bgfx::UniformType::Vec4
#define Binding_Float4 bgfx::UniformType::Vec4
#define Binding_Texture2D bgfx::UniformType::Sampler
#define Binding(_name, _type) bgfx::UniformHandle _name = bgfx::createUniform(#_name, _type);
#include "shaders/bindings.def"
#undef Binding
#undef Binding_Float
#undef Binding_Float3
#undef Binding_Float4
#undef Binding_Texture2D
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
struct ShaderBindingPointUniforms;
struct BGFXCallbacks;
struct FrameRenderer;
struct Window;
class ShaderCompilerModule;
struct ICapture;
struct Context {
public:
	std::shared_ptr<ShaderCompilerModule> shaderCompilerModule;
	std::shared_ptr<ShaderBindingPointUniforms> bindingUniforms;
	std::shared_ptr<BGFXCallbacks> bgfxCallbacks;
	std::shared_ptr<ImguiContext> imguiContext;

	// ShaderCache shaderCache;
	// ShaderBindingPoints bindingPoints;

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
