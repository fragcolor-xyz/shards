#include "context.hpp"
#include <memory>
#include <string>
#include <filesystem>
#include "SDL_events.h"
#include "SDL_keycode.h"
#include "../bgfx/context.hpp"
#include "../bgfx.hpp"
#include "chainblocks.hpp"
#include "mesh.hpp"
#include "material.hpp"
#include "imgui.hpp"
#include "sdl_native_window.hpp"
#include <bgfx/bgfx.h>
#include <bx/debug.h>
#include <bx/string.h>
#include <stb_image_write.h>

namespace gfx {
struct BGFXCallbacks : public bgfx::CallbackI {
	std::string fatalError;

	virtual ~BGFXCallbacks() {}
	virtual void fatal(const char *_filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char *_str) override;
	virtual void traceVargs(const char *_filePath, uint16_t _line, const char *_format, va_list _argList) override;
	virtual void profilerBegin(const char * /*_name*/, uint32_t /*_abgr*/, const char * /*_filePath*/, uint16_t /*_line*/) override {}
	virtual void profilerBeginLiteral(const char * /*_name*/, uint32_t /*_abgr*/, const char * /*_filePath*/, uint16_t /*_line*/) override {}
	virtual void profilerEnd() override {}
	virtual uint32_t cacheReadSize(uint64_t /*_id*/) override { return 0; }
	virtual bool cacheRead(uint64_t /*_id*/, void * /*_data*/, uint32_t /*_size*/) override { return false; }
	virtual void cacheWrite(uint64_t /*_id*/, const void * /*_data*/, uint32_t /*_size*/) override {}
	virtual void screenShot(const char *_filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void *_data, uint32_t _size, bool _yflip) override;
	virtual void captureBegin(uint32_t /*_width*/, uint32_t /*_height*/, uint32_t /*_pitch*/, bgfx::TextureFormat::Enum /*_format*/, bool /*_yflip*/) override;
	virtual void captureEnd() override {}
	virtual void captureFrame(const void * /*_data*/, uint32_t /*_size*/) override {}
};

Context::Context() {
	imguiContext = std::make_shared<ImguiContext>();
}

void Context::init(CBContext *context, SDL_Window *window) {
	this->window = window;

	// get the window size again to ensure it's correct
	SDL_GetWindowSize(window, &width, &height);

	CBLOG_DEBUG("GFX.Context init width: {} height: {}", width, height);

#ifdef __APPLE__
	_metalView = SDL_Metal_CreateView(window);
	if (!_fsMode) {
		// seems to work only when windowed...
		// tricky cos retina..
		// find out the scaling
		int real_w, real_h;
		SDL_Metal_GetDrawableSize(window, &real_w, &real_h);
		_windowScalingW = float(real_w) / float(width);
		_windowScalingH = float(real_h) / float(height);
		// fix the scaling now if needed
		if (_windowScalingW != 1.0 || _windowScalingH != 1.0) {
			SDL_Metal_DestroyView(_metalView);
			SDL_SetWindowSize(window, int(float(width) / _windowScalingW), int(float(height) / _windowScalingH));
			_metalView = SDL_Metal_CreateView(window);
		}
	}
	else {
		int real_w, real_h;
		SDL_Metal_GetDrawableSize(window, &real_w, &real_h);
		_windowScalingW = float(real_w) / float(width);
		_windowScalingH = float(real_h) / float(height);
		// finally in this case override our real values
		width = real_w;
		height = real_h;
	}
	nativeWindow = (void *)SDL_Metal_GetLayer(_metalView);
#elif defined(__EMSCRIPTEN__)
	nativeWindow = (void *)("#canvas"); // SDL and emscripten use #canvas
#elif defined(_WIN32) || defined(__linux__)
	nativeWindow = (void *)SDL_GetNativeWindowPtr(window);
#endif

	bgfxCallbacks = std::make_shared<BGFXCallbacks>();

	bgfx::Init initInfo{};
	initInfo.callback = bgfxCallbacks.get();
	initInfo.type = getBgfxRenderType();

	// set platform data this way.. or we will have issues if we re-init bgfx
	bgfx::PlatformData pdata{};
	pdata.nwh = nativeWindow;
	pdata.ndt = SDL_GetNativeDisplayPtr(window);
	bgfx::setPlatformData(pdata);

	initInfo.resolution.width = width;
	initInfo.resolution.height = height;
	initInfo.resolution.reset = resetFlags;
	initInfo.debug = true;
	if (!bgfx::init(initInfo)) {
		throw ActivationError("Failed to initialize BGFX");
	}

	shaderCompiler = chainblocks::makeShaderCompiler();
	shaderCompiler->warmup(context);

	bindingUniforms = std::make_shared<ShaderBindingPointUniforms>();

#ifdef BGFX_CONFIG_RENDERER_OPENGL_MIN_VERSION
	CBLOG_INFO("Renderer version: {}", bgfx::getRendererName(bgfx::RendererType::OpenGL));
#elif BGFX_CONFIG_RENDERER_OPENGLES_MIN_VERSION
	CBLOG_INFO("Renderer version: {}", bgfx::getRendererName(bgfx::RendererType::OpenGLES));
#endif

	imguiContext->init(18.0, GuiViewId);

	_timeUniformHandle = bgfx::createUniform("u_private_time4", bgfx::UniformType::Vec4, 1);

	// setup main view
	bgfx::setViewRect(0, 0, 0, width, height);
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, Var(clearColor).colorToInt(), 1.0f, 0);
}

void Context::cleanup() {
	for (auto &sampler : _samplers) {
		bgfx::destroy(sampler);
	}
	_samplers.clear();

	// _pickingRT is destroyed when FB is destroyed!
	if (_pickingUniform.idx != bgfx::kInvalidHandle) {
		bgfx::destroy(_pickingUniform);
		_pickingUniform = BGFX_INVALID_HANDLE;
	}
	if (_pickingTexture.idx != bgfx::kInvalidHandle) {
		bgfx::destroy(_pickingTexture);
		_pickingTexture = BGFX_INVALID_HANDLE;
	}

#ifdef __APPLE__
	if (_metalView) {
		SDL_Metal_DestroyView(_metalView);
		_metalView = nullptr;
	}
#endif
}

const bgfx::UniformHandle &Context::getSampler(size_t index) {
	const auto nsamplers = _samplers.size();
	if (index >= nsamplers) {
		std::string name("DrawSampler_");
		name.append(std::to_string(index));
		return _samplers.emplace_back(bgfx::createUniform(name.c_str(), bgfx::UniformType::Sampler));
	}
	else {
		return _samplers[index];
	}
}

void Context::resizeMainOutput(int32_t width, int32_t height) {
	CBLOG_DEBUG("GFX.Context resized width: {} height: {}", width, height);
	this->width = width;
	this->height = height;
	bgfx::reset(width, height, resetFlags);
}

void Context::resizeMainOutputConditional(int32_t width, int32_t height) {
	if (width != this->width || height != this->height) {
		resizeMainOutput(width, height);
	}
}

const bgfx::UniformHandle Context::getPickingUniform() {
	if (_pickingUniform.idx == bgfx::kInvalidHandle) {
		_pickingUniform = bgfx::createUniform("u_picking_id", bgfx::UniformType::Vec4);
	}
	return _pickingUniform;
}

void Context::setupPicking() {
	// Set up ID buffer, which has a color target and depth buffer
	pickingRenderTarget() = bgfx::createTexture2D(
		PickingBufferSize, PickingBufferSize, false, 1, bgfx::TextureFormat::RGBA8,
		0 | BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
	auto pickingDepthRT = bgfx::createTexture2D(
		PickingBufferSize, PickingBufferSize, false, 1, bgfx::TextureFormat::D32F,
		0 | BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

	// CPU texture for blitting to and reading ID buffer so we can see what was
	// clicked on. Impossible to read directly from a render target, you *must*
	// blit to a CPU texture first. Algorithm Overview: Render on GPU -> Blit to
	// CPU texture -> Read from CPU texture.
	pickingTexture() = bgfx::createTexture2D(
		PickingBufferSize, PickingBufferSize, false, 1, bgfx::TextureFormat::RGBA8,
		0 | BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP |
			BGFX_SAMPLER_V_CLAMP);

	bgfx::TextureHandle rt[2] = {pickingRenderTarget(), pickingDepthRT};
	_pickingFB = bgfx::createFrameBuffer(BX_COUNTOF(rt), rt, true);
	bgfx::setViewRect(PickingViewId, 0, 0, PickingBufferSize, PickingBufferSize);
	bgfx::setViewClear(PickingViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
}

void Context::beginFrame(FrameRenderer *frameRenderer) {
	if (_currentFrameRenderer != nullptr) throw ActivationError("Frame already being rendered");
	_currentFrameRenderer = frameRenderer;
}

void Context::endFrame(FrameRenderer *frameRenderer) {
	assert(_currentFrameRenderer == frameRenderer);
	_currentFrameRenderer = nullptr;
}

void BGFXCallbacks::fatal(const char *_filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char *_str) {
	if (bgfx::Fatal::DebugCheck == _code) {
		bx::debugBreak();
	}
	else {
		fatalError = _str;
		throw std::runtime_error(_str);
	}
}

void BGFXCallbacks::traceVargs(const char *_filePath, uint16_t _line, const char *_format, va_list _argList) {
	char temp[2048];
	char *out = temp;
	va_list argListCopy;
	va_copy(argListCopy, _argList);
	int32_t len = bx::snprintf(out, sizeof(temp), "%s (%d): ", _filePath, _line);
	int32_t total = len + bx::vsnprintf(out + len, sizeof(temp) - len, _format, argListCopy);
	va_end(argListCopy);
	if ((int32_t)sizeof(temp) < total) {
		out = (char *)alloca(total + 1);
		bx::memCopy(out, temp, len);
		bx::vsnprintf(out + len, total - len, _format, _argList);
	}
	out[total] = '\0';
	CBLOG_DEBUG("(bgfx): {}", out);
}

void BGFXCallbacks::screenShot(const char *_filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void *_data, uint32_t _size, bool _yflip) {
	CBLOG_DEBUG("Screenshot requested for path:  {}", _filePath);

	// For now.. TODO as might be not true
	assert(_pitch == (_width * 4));

	// need to convert to rgba
	std::vector<uint8_t> data;
	data.resize(_size);
	const uint8_t *bgra = (uint8_t *)_data;
	for (uint32_t x = 0; x < _height; x++) {
		for (uint32_t y = 0; y < _width; y++) {
			data[((y * 4) + 0) + (_pitch * x)] = bgra[((y * 4) + 2) + (_pitch * x)];
			data[((y * 4) + 1) + (_pitch * x)] = bgra[((y * 4) + 1) + (_pitch * x)];
			data[((y * 4) + 2) + (_pitch * x)] = bgra[((y * 4) + 0) + (_pitch * x)];
			data[((y * 4) + 3) + (_pitch * x)] = bgra[((y * 4) + 3) + (_pitch * x)];
		}
	}

	stbi_flip_vertically_on_write(_yflip);
	std::filesystem::path p(_filePath);
	std::filesystem::path t = p;
	t += ".tmp";
	const auto tname = t.string();
	const auto ext = p.extension();
	if (ext == ".png" || ext == ".PNG") {
		stbi_write_png(tname.c_str(), _width, _height, 4, data.data(), _pitch);
	}
	else if (ext == ".jpg" || ext == ".JPG" || ext == ".jpeg" || ext == ".JPEG") {
		stbi_write_jpg(tname.c_str(), _width, _height, 4, data.data(), 95);
	}
	// we do this to ensure p is a complete flushed file
	std::filesystem::rename(t, p);
}

void BGFXCallbacks::captureBegin(uint32_t /*_width*/, uint32_t /*_height*/, uint32_t /*_pitch*/, bgfx::TextureFormat::Enum /*_format*/, bool /*_yflip*/) {
	BX_TRACE("Warning: using capture without callback (a.k.a. pointless).");
}

} // namespace gfx

#include "fs_picking.bin.h"
bgfx::EmbeddedShader gfx::Context::PickingShaderData = BGFX_EMBEDDED_SHADER(fs_picking);
