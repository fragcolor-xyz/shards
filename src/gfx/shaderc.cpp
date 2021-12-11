#include "context.hpp"
#include "paths.hpp"
#include "shaderc/api.hpp"
#include "spdlog/spdlog.h"
#include <bx/filepath.h>
#include <bx/os.h>
#include <bx/platform.h>
#include <cassert>
#include <gfx/shader_paths.hpp>

namespace gfx {

void ShaderCompileOutput::writeError(const char *error) { errors.push_back(error); }
void ShaderCompileOutput::writeOutput(const void *data, int32_t size) {
	size_t offset = binary.size();
	binary.resize(offset + size);
	memcpy(&binary[offset], data, size);
}

ShaderCompileOptions::ShaderCompileOptions() { setBuiltinIncludePaths(); }

void ShaderCompileOptions::setBuiltinIncludePaths() {
	for (auto path : shaderIncludePaths) {
		includeDirs.push_back(resolveDataPath(path));
	}
}
void ShaderCompileOptions::setCompatibleForContext(Context &context) {
	setCompatibleForRendererType(context.getRendererType());
}

void ShaderCompileOptions::setCompatibleForRendererType(RendererType type) {
#if BX_PLATFORM_WINDOWS
	platform = "windows";
#elif BX_PLATFORM_EMSCRIPTEN
	platform = "asm.js";
#elif BX_PLATFORM_LINUX
	platform = "linux";
#elfi BX_PLATFORM_OSX
	platform = "osx";
#elif BX_PLATFORM_IOS
	platform = "ios";
#endif

	switch (type) {
	case RendererType::DirectX11:
		if (shaderType == 'v') {
			profile = "vs_5_0";
		} else if (shaderType == 'f') {
			profile = "ps_5_0";
		} else if (shaderType == 'c') {
			profile = "cs_5_0";
		}
		break;
	case RendererType::OpenGL:
#if BX_PLATFORM_EMSCRIPTEN
		profile = "300_es";
#else
		profile = "430";
#endif
		break;
	case RendererType::Metal:
		profile = "metal";
		break;
	case RendererType::Vulkan:
		profile = "spirv";
		break;
	default:
		assert(false);
	}
}

ShaderCompilerModule::ShaderCompilerModule(IShaderCompiler *instance, void *moduleHandle) : instance(instance), moduleHandle(moduleHandle) {}
ShaderCompilerModule::~ShaderCompilerModule() {
	if (instance) {
		delete instance;
	}
	if (moduleHandle) {
		bx::dlclose(moduleHandle);
	}
}
IShaderCompiler &ShaderCompilerModule::getShaderCompiler() {
	assert(instance);
	return *instance;
}

#ifdef GFX_SHADERC_STATIC
extern "C" {
gfx::IShaderCompiler *GFX_SHADERC_MODULE_ENTRY();
}
#endif

std::shared_ptr<ShaderCompilerModule> createShaderCompilerModule() {
#ifdef GFX_SHADERC_STATIC
	return std::make_shared<ShaderCompilerModule>(GFX_SHADERC_MODULE_ENTRY());
#else
	void *module = nullptr;

#if _WIN32
	if (!module)
		module = bx::dlopen("gfx-shaderc.dll");
	if (!module)
		module = bx::dlopen("libgfx-shaderc.dll");
#else
	if (!module)
		module = bx::dlopen("libgfx-shaderc.so");
#endif

	if (!module) {
		spdlog::error("Failed to load shader compiler module");
		return std::shared_ptr<ShaderCompilerModule>();
	}

#define _EXPAND(_X) #_X
#define EXPAND(_X) _EXPAND(_X)
	const char *symbolName = EXPAND(GFX_SHADERC_MODULE_ENTRY);
	gfx::f_GetShaderCompilerInterface entry = (gfx::f_GetShaderCompilerInterface)bx::dlsym(module, symbolName);

	if (!entry) {
		spdlog::error("Failed to find entry point in shader compiler module");
		bx::dlclose(module);
		return std::shared_ptr<ShaderCompilerModule>();
	}

	return std::make_shared<ShaderCompilerModule>(entry(), module);
#endif
}
} // namespace gfx