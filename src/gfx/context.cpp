#include "context.hpp"
#include "SDL_events.h"
#include "SDL_keycode.h"
#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "capture.hpp"
#include "imgui.hpp"
#include "magic_enum.hpp"
#include "material.hpp"
#include "material_shader.hpp"
#include "mesh.hpp"
#include "sdl_native_window.hpp"
#include "shaderc.hpp"
#include "spdlog/spdlog.h"
#include "window.hpp"
#include <algorithm>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/debug.h>
#include <bx/string.h>
#include <exception>
#include <filesystem>
#include <magic_enum.hpp>
#include <memory>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#pragma GCC diagnostic pop

#ifdef __EMSCRIPTEN__
#define BGFX_CONFIG_MULTITHREADED 0
#else
#define BGFX_CONFIG_MULTITHREADED 1
#endif

#if BGFX_CONFIG_MULTITHREADED
#include "bx/semaphore.h"
#endif

namespace gfx {

#if 0
constexpr RendererType DefaultRenderer = RendererType::Vulkan;
#elif BX_PLATFORM_LINUX || BX_PLATFORM_EMSCRIPTEN
constexpr RendererType DefaultRenderer = RendererType::OpenGL;
#elif BX_PLATFORM_WINDOWS
constexpr RendererType DefaultRenderer = RendererType::DirectX11;
#elif BX_PLATFORM_OSX || BX_PLATFORM_IOS
constexpr RendererType DefaultRenderer = RendererType::Metal;
#endif

static std::string formatBGFXException(const char *filepath, uint16_t line, bgfx::Fatal::Enum code, const char *str) {
	return fmt::format("BGFX fatal {}:{}: {}", filepath, line, str);
}

BGFXException::BGFXException(const char *filepath, uint16_t line, bgfx::Fatal::Enum code, const char *str)
	: std::runtime_error(formatBGFXException(filepath, line, code, str)), line(line), code(code) {}

struct BGFXCallbacks : public bgfx::CallbackI {
#if BGFX_CONFIG_MULTITHREADED
	std::atomic<uint32_t> waitForCaptureCount = 0;
	bx::Semaphore captureSemaphore;
#endif

	CaptureFormat captureFormat;
	std::vector<std::weak_ptr<ICapture>> captureCallbacks;

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
	virtual void captureEnd() override;
	virtual void captureFrame(const void * /*_data*/, uint32_t /*_size*/) override;
};

ContextCreationOptions::ContextCreationOptions() {
#ifdef GFX_DEBUG
	debug = true;
#endif
}

Context::Context() {}
Context::~Context() { cleanup(); }

void Context::init(Window &window, const ContextCreationOptions &options) {
	spdlog::debug("GFX.Context init");
	assert(!isInitialized());

	this->window = &window;

	bgfxCallbacks = std::make_shared<BGFXCallbacks>();

	bgfx::PlatformData pdata{};
	if (options.overrideNativeWindowHandle) {
		pdata.nwh = options.overrideNativeWindowHandle;
	} else {
		pdata.nwh = window.getNativeWindowHandle();
	}
	pdata.ndt = SDL_GetNativeDisplayPtr(window.window);
	bgfx::setPlatformData(pdata);

	RendererType rendererType;
	if (options.renderer != RendererType::None) {
		rendererType = options.renderer;
	} else {
		RendererType envRendererType = RendererType::None;
		if (const char *gfxRenderer = SDL_getenv("GFX_RENDERER")) {
			envRendererType = getRendererTypeByName(gfxRenderer);
		}

		if (envRendererType != RendererType::None) {
			rendererType = envRendererType;
		} else {
			rendererType = DefaultRenderer;
		}
	}

	spdlog::debug("Set renderer type to {}", getRendererTypeName(rendererType));

	bgfx::Init initInfo{};
	initInfo.callback = bgfxCallbacks.get();
	initInfo.type = getBgfxRenderType(rendererType);

	mainOutputSize = window.getDrawableSize();
	initInfo.resolution.width = mainOutputSize.x;
	initInfo.resolution.height = mainOutputSize.y;
	initInfo.resolution.reset = resetFlags;
	initInfo.resolution.format = options.backbufferFormat;
	initInfo.debug = options.debug;
	initInfo.profile = options.debug;
	if (!bgfx::init(initInfo)) {
		throw std::runtime_error("Failed to initialize BGFX");
	}
	this->rendererType = rendererType;

	timeUniformHandle = bgfx::createUniform("u_private_time4", bgfx::UniformType::Vec4, 1);
}

void Context::cleanup() {
	spdlog::debug("GFX.Context cleanup");

	if (isInitialized()) {
		rendererType = RendererType::None;

		for (auto &sampler : samplers) {
			bgfx::destroy(sampler);
		}
		samplers.clear();

		bgfx::shutdown();
	}
}

const bgfx::UniformHandle &Context::getSampler(size_t index) {
	const auto nsamplers = samplers.size();
	if (index >= nsamplers) {
		std::string name("DrawSampler_");
		name.append(std::to_string(index));
		return samplers.emplace_back(bgfx::createUniform(name.c_str(), bgfx::UniformType::Sampler));
	} else {
		return samplers[index];
	}
}

void Context::resizeMainOutput(const int2 &newSize) {
	spdlog::debug("GFX.Context resized width: {} height: {}", newSize.x, newSize.y);
	this->mainOutputSize = newSize;
	bgfx::reset(newSize.x, newSize.y, resetFlags);
}

void Context::resizeMainOutputConditional(const int2 &newSize) {
	if (mainOutputSize != newSize) {
		resizeMainOutput(newSize);
	}
}

void Context::addCapture(std::weak_ptr<ICapture> capture) {
	auto &captureCallbacks = bgfxCallbacks->captureCallbacks;

	bool wasCaptureEnabled = captureCallbacks.size() > 0;

	captureCallbacks.push_back(capture);

	if (!wasCaptureEnabled) {
		spdlog::debug("Enabling BGFX capture callback");

		resetFlags |= BGFX_RESET_CAPTURE;
		bgfx::reset(mainOutputSize.x, mainOutputSize.y, resetFlags);
	}
}

void Context::removeCapture(std::weak_ptr<ICapture> capture) {
	auto &captureCallbacks = bgfxCallbacks->captureCallbacks;

	bool wasCaptureEnabled = captureCallbacks.size() > 0;

	auto filter = [&](const std::weak_ptr<ICapture> &item) {
		if (item.expired())
			return true;
		if (!capture.expired() && capture.lock() == item.lock()) {
			return true;
		}
		return false;
	};

	captureCallbacks.erase(std::remove_if(captureCallbacks.begin(), captureCallbacks.end(), filter));

	bool disableCapture = wasCaptureEnabled && captureCallbacks.size() == 0;
	if (disableCapture) {
		spdlog::debug("Disabling BGFX capture callback");

		resetFlags = resetFlags & (~BGFX_RESET_CAPTURE);
		bgfx::reset(mainOutputSize.x, mainOutputSize.y, resetFlags);
	}
}

void Context::captureMark() {
	assert(bgfxCallbacks->captureCallbacks.size() > 0);
#if BGFX_CONFIG_MULTITHREADED
	bgfxCallbacks->waitForCaptureCount += 1;
#endif
}

void Context::captureSync(uint32_t timeout) {
	assert(bgfxCallbacks->captureCallbacks.size() > 0);
#if BGFX_CONFIG_MULTITHREADED
	bgfxCallbacks->captureSemaphore.wait(timeout);
	bgfxCallbacks->waitForCaptureCount = 0;
#endif
}

void Context::beginFrame(FrameRenderer *frameRenderer) {
	if (currentFrameRenderer != nullptr)
		throw std::runtime_error("Frame already being rendered");
	currentFrameRenderer = frameRenderer;
}

void Context::endFrame(FrameRenderer *frameRenderer) {
	assert(currentFrameRenderer == frameRenderer);
	currentFrameRenderer = nullptr;
}

void BGFXCallbacks::fatal(const char *_filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char *_str) {
	spdlog::error("{}", formatBGFXException(_filePath, _line, _code, _str));
	if (bgfx::Fatal::DebugCheck == _code) {
		bx::debugBreak();
	} else {
		throw BGFXException(_filePath, _line, _code, _str);
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
	spdlog::debug("{}", out);
}

void BGFXCallbacks::screenShot(const char *_filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void *_data, uint32_t _size, bool _yflip) {
	spdlog::debug("Screenshot requested for path:  {}", _filePath);

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
	} else if (ext == ".jpg" || ext == ".JPG" || ext == ".jpeg" || ext == ".JPEG") {
		stbi_write_jpg(tname.c_str(), _width, _height, 4, data.data(), 95);
	}
	// we do this to ensure p is a complete flushed file
	std::filesystem::rename(t, p);
}

void BGFXCallbacks::captureBegin(uint32_t width, uint32_t height, uint32_t pitch, bgfx::TextureFormat::Enum format, bool yflip) {
	captureFormat.size = int2(width, height);
	captureFormat.pitch = pitch;
	captureFormat.format = format;
	captureFormat.yflip = yflip;
}

void BGFXCallbacks::captureEnd() {}

void BGFXCallbacks::captureFrame(const void *data, uint32_t size) {
	for (auto &callbackPtr : captureCallbacks) {
		auto callback = callbackPtr.lock();
		if (callback) {
			callback->frame(captureFormat, data, size);
		}
	}

#if BGFX_CONFIG_MULTITHREADED
	if (waitForCaptureCount > 0) {
		captureSemaphore.post(waitForCaptureCount);
	}
#endif
}

} // namespace gfx
