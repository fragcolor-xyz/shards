#pragma once

#include <boost/align/aligned_allocator.hpp>
#include <unordered_map>
#include <deque>
#include <memory>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>
#include <bgfx/embedded_shader.h>
#include "shader_cache.hpp"
#include <chainblocks.hpp>
#include "linalg_shim.hpp"

struct CBContext;
struct CBChain;

namespace chainblocks {
struct IShaderCompiler;
}

namespace gfx {
using chainblocks::Mat4;

constexpr uint16_t PickingBufferSize = 128;
constexpr uint32_t MaxNumLights = 4;

// BGFX_CONFIG_MAX_VIEWS is 256
constexpr bgfx::ViewId MaxViews = 256;
constexpr bgfx::ViewId GuiViewId = MaxViews - 1;
constexpr bgfx::ViewId BlittingViewId = GuiViewId - 1;
constexpr bgfx::ViewId PickingViewId = BlittingViewId - 1;

struct IDrawable {
	virtual CBChain *getChain() = 0;
};

enum class Renderer { None, DirectX11, Vulkan, OpenGL, Metal };

#if defined(BGFX_CONFIG_RENDERER_VULKAN)
constexpr Renderer CurrentRenderer = Renderer::Vulkan;
#elif defined(__linux__) || defined(__EMSCRIPTEN__) || defined(BGFX_CONFIG_RENDERER_OPENGL)
constexpr Renderer CurrentRenderer = Renderer::OpenGL;
#elif defined(_WIN32)
constexpr Renderer CurrentRenderer = Renderer::DirectX11;
#elif defined(__APPLE__)
constexpr Renderer CurrentRenderer = Renderer::Metal;
#endif

inline bgfx::RendererType::Enum getBgfxRenderType() {
	switch (CurrentRenderer) {
	case Renderer::DirectX11:
		return bgfx::RendererType::Direct3D11;
	case Renderer::Vulkan:
		return bgfx::RendererType::Vulkan;
	case Renderer::OpenGL:
		return bgfx::RendererType::OpenGL;
	case Renderer::Metal:
		return bgfx::RendererType::Metal;
	default:
		return bgfx::RendererType::Noop;
	}
}

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

struct ImguiContext;
struct Primitive;
struct ShaderBindingPointUniforms;
struct BGFXCallbacks;
struct FrameRenderer;
struct Context {
	std::unique_ptr<chainblocks::IShaderCompiler> shaderCompiler;
	std::shared_ptr<ShaderBindingPointUniforms> bindingUniforms;
	std::shared_ptr<BGFXCallbacks> bgfxCallbacks;
	std::shared_ptr<ImguiContext> imguiContext;
	ShaderCache shaderCache;
	ShaderBindingPoints bindingPoints;
	uint32_t resetFlags = BGFX_RESET_VSYNC;
	CBColor clearColor;

#if defined(__APPLE__)
	SDL_MetalView _metalView{nullptr};
#endif

	int32_t width, height;

	float _windowScalingW{1.0};
	float _windowScalingH{1.0};

	SDL_Window *window = nullptr;
	void *nativeWindow = nullptr;

private:
	FrameRenderer *_currentFrameRenderer = nullptr;

public:
	Context();
	void init(CBContext *cbContext, SDL_Window *window);
	void cleanup();

	// ### CLEANUP BELOW
	static bgfx::EmbeddedShader PickingShaderData;

	// Useful to compare with with plugins, they might mismatch!
	const static inline uint32_t BgfxABIVersion = BGFX_API_VERSION;

	const bgfx::UniformHandle &getSampler(size_t index);

	void resizeMainOutput(int32_t width, int32_t height);
	void resizeMainOutputConditional(int32_t width, int32_t height);
	
	bgfx::TextureHandle &pickingTexture() { return _pickingTexture; }
	bgfx::TextureHandle &pickingRenderTarget() { return _pickingRT; }

	const bgfx::UniformHandle getPickingUniform();
	void setupPicking();

	std::vector<bgfx::UniformHandle> _samplers;

	bgfx::UniformHandle _pickingUniform = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _pickingTexture = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _pickingRT = BGFX_INVALID_HANDLE;
	bgfx::FrameBufferHandle _pickingFB = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle _timeUniformHandle = BGFX_INVALID_HANDLE;
	// ### CLEANUP ABOVE

public:
	void beginFrame(FrameRenderer *frameRenderer);
	void endFrame(FrameRenderer *frameRenderer);
	FrameRenderer *getFrameRenderer() { return _currentFrameRenderer; }
};

} // namespace gfx
