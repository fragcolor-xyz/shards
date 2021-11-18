#include "gfx/paths.hpp"
#include "gfx/shaderc.hpp"
#include "spdlog/spdlog.h"
#include <bx/platform.h>
#include <bx/string.h>
#include <cassert>
#include <catch2/catch_all.hpp>
#include <cstring>

using namespace gfx;

void testCompileSimpleShader(const char *platform, const char *profile) {
	const char *varyings = R"(vec2 v_texcoord0 : TEXCOORD0;
)";

	const char *fragCode = R"($input v_texcoord0
#include <bgfx_shader.sh>
void main() {
  gl_FragColor = vec4(v_texcoord0.x, v_texcoord0.y, 0, 1);
}
)";

	ShaderCompileOptions options;

	options.shaderType = 'f';
	options.debugInformation = true;
	options.disasm = true;
	options.optimize = false;

	options.platform = platform;
	options.profile = profile;

	auto shaderCompilerModule = gfx::createShaderCompilerModule();
	CHECK(shaderCompilerModule);

	IShaderCompiler &shaderCompiler = shaderCompilerModule->getShaderCompiler();

	ShaderCompileOutput output;
	CHECK(shaderCompiler.compile(options, output, varyings, fragCode, strlen(fragCode)));
	CHECK(output.binary.size() > 0);
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#define LINUX
#endif

#if BX_PLATFORM_WINDOWS
TEST_CASE("windows/ps_5_0", "[shaderc]") { testCompileSimpleShader("windows", "ps_5_0"); }
TEST_CASE("windows/spirv", "[shaderc]") { testCompileSimpleShader("windows", "spirv"); }
#endif

#define NON_WINDOWS_DESKTOP_PLATFORM (BX_PLATFORM_WINDOWS || BX_PLATFORM_LINUX || BX_PLATFORM_OSX)

#if NON_WINDOWS_DESKTOP_PLATFORM
TEST_CASE("linux/430", "[shaderc]") { testCompileSimpleShader("linux", "430"); }

TEST_CASE("linux/metal", "[shaderc]") { testCompileSimpleShader("linux", "metal"); }

TEST_CASE("linux/spirv", "[shaderc]") { testCompileSimpleShader("linux", "spirv"); }
#endif

#if NON_WINDOWS_DESKTOP_PLATFORM || BX_PLATFORM_IOS
TEST_CASE("ios/metal", "[shaderc]") { testCompileSimpleShader("ios", "metal"); }
#endif

#if NON_WINDOWS_DESKTOP_PLATFORM || BX_PLATFORM_EMSCRIPTEN
TEST_CASE("asm.js/300_es", "[shaderc]") { testCompileSimpleShader("asm.js", "300_es"); }
#endif
