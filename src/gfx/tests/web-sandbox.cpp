#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "gfx/context.hpp"
#include "gfx/frame.hpp"
#include "gfx/geom.hpp"
#include "gfx/imgui.hpp"
#include "gfx/loop.hpp"
#include "gfx/mesh.hpp"
#include "gfx/paths.hpp"
#include "gfx/shaderc.hpp"
#include "gfx/types.hpp"
#include "gfx/view.hpp"
#include "gfx/window.hpp"
#include "imgui.h"
#include "spdlog/spdlog.h"
#include <SDL_events.h>
#include <bx/string.h>
#include <cassert>
#include <cstring>
#include <exception>
#include <string>

using namespace gfx;

struct App {
	gfx::Window window;
	gfx::Context context;
	gfx::Loop loop;

	gfx::Primitive prim;
	bgfx::ProgramHandle prog;

	void dumpShaderSource(std::vector<uint8_t> data) {
		const uint8_t *dataPtr = data.data();
		const uint8_t *dataEndPtr = data.data() + data.size();

		bx::MemoryReader reader(dataPtr, data.size());
		uint32_t m_hash = bx::hash<bx::HashMurmur2A>(dataPtr, data.size());

		uint32_t magic;
		bx::read(&reader, magic);

		uint32_t hashIn;
		bx::read(&reader, hashIn);

		uint32_t hashOut;
		bx::read(&reader, hashOut);

		uint16_t count;
		bx::read(&reader, count);

		for (uint32_t ii = 0; ii < count; ++ii) {
			uint8_t nameSize = 0;
			bx::read(&reader, nameSize);

			char name[256];
			bx::read(&reader, &name, nameSize);
			name[nameSize] = '\0';

			uint8_t type;
			bx::read(&reader, type);

			uint8_t num;
			bx::read(&reader, num);

			uint16_t regIndex;
			bx::read(&reader, regIndex);

			uint16_t regCount;
			bx::read(&reader, regCount);

			uint16_t texInfo = 0;
			bx::read(&reader, texInfo);

			uint16_t texFormat = 0;
			bx::read(&reader, texFormat);
		}

		uint32_t shaderSize;
		bx::read(&reader, shaderSize);

		const uint8_t *sourceDataPtr = reader.getDataPtr();

		std::string str(sourceDataPtr, sourceDataPtr + shaderSize);
		spdlog::info("Shader source: \n{}", str);
	}

	void init(const char *hostElement) {
		spdlog::info("sandbox Init");
		window.init();
		gfx::ContextCreationOptions contextOptions;
		contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
		context.init(window, contextOptions);

		spdlog::info("sandbox start user code");
		gfx::Color clearColor = gfx::Color(0, 0, 0, 255);

		gfx::geom::SphereGenerator gen;
		gen.generate();

		const char vars[] = R"(
vec3 v_normal    : NORMAL    = vec3(0.0, 0.0, 1.0);
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);

vec3 a_position  : POSITION;
vec2 a_texcoord0 : TEXCOORD0;
vec3 a_normal    : NORMAL;
)";
		const char vsCode[] = R"(
$input a_position, a_normal, a_texcoord0
$output v_normal, v_texcoord0

#include <bgfx_shader.sh>
void main() {
	v_texcoord0 = a_texcoord0;
	v_normal = a_normal;
	gl_Position = mul(u_viewProj, mul(u_model[0], vec4(a_position, 1.0)));
}
)";
		const char psCode[] = R"(
$input v_normal, v_texcoord0

#include <bgfx_shader.sh>
void main() {
	gl_FragColor = vec4(v_texcoord0.x, v_texcoord0.y, 0, 1);
}
)";

		gfx::IShaderCompiler &shaderCompiler = context.shaderCompilerModule->getShaderCompiler();
		gfx::ShaderCompileOptions shadercOptions;
		gfx::ShaderCompileOutput shadercOutput;

		spdlog::info("sandbox Compile vertex shader");

		shadercOutput.binary.clear();
		shadercOptions.shaderType = 'v';
		shadercOptions.setCompatibleForContext(context);
		assert(shaderCompiler.compile(shadercOptions, shadercOutput, vars, vsCode, sizeof(vsCode)));
		bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(shadercOutput.binary.data(), shadercOutput.binary.size()));
		dumpShaderSource(shadercOutput.binary);

		spdlog::info("sandbox Compile fragment shader");

		shadercOutput.binary.clear();
		shadercOptions.shaderType = 'f';
		shadercOptions.setCompatibleForContext(context);
		assert(shaderCompiler.compile(shadercOptions, shadercOutput, vars, psCode, sizeof(psCode)));
		bgfx::ShaderHandle ps = bgfx::createShader(bgfx::copy(shadercOutput.binary.data(), shadercOutput.binary.size()));
		dumpShaderSource(shadercOutput.binary);

		spdlog::info("sandbox Create program");

		prog = bgfx::createProgram(vs, ps, false);

		const bgfx::Memory *vertMem = bgfx::copy(gen.vertices.data(), gen.vertices.size() * sizeof(gfx::geom::VertexPNT));
		prim.vb = bgfx::createVertexBuffer(vertMem, gfx::geom::VertexPNT::getVertexLayout());
		const bgfx::Memory *indexMem = bgfx::copy(gen.indices.data(), gen.indices.size() * sizeof(gfx::geom::GeneratorBase::index_t));
		prim.ib = bgfx::createIndexBuffer(indexMem);
		prim.staticUsageParameters.flags |= gfx::MaterialUsageFlags::HasNormals;
		prim.pipelineParameters.primitiveType = gfx::MeshPrimitiveType::TriangleList;
	}

	void tick() {
		spdlog::info("sandbox tick");
		bool quit = false;
		float deltaTime;
		if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {
			std::vector<SDL_Event> events;
			window.pollEvents(events);
			for (auto &event : events) {
				if (event.type == SDL_QUIT)
					quit = true;
			}

			gfx::FrameRenderer frame(context, gfx::FrameInputs(deltaTime, loop.getAbsoluteTime(), 0, events));
			frame.begin();

			gfx::View &view = frame.pushMainOutputView();
			bgfx::setViewClear(view.id, BGFX_CLEAR_COLOR, colorToRGBA(Color(10, 10, 10, 255)));
			bgfx::Transform transformCache;

			gfx::float4x4 modelMtx = linalg::identity;
			gfx::float4x4 projMtx = linalg::perspective_matrix(pi * 0.6f, float(view.getSize().x) / view.getSize().y, 0.01f, 100.0f);
			gfx::float4x4 viewMtx = linalg::translation_matrix(gfx::float3(0, 0, -5));

			uint32_t transformCacheIndex = bgfx::allocTransform(&transformCache, 3);
			gfx::packFloat4x4(modelMtx, transformCache.data + 16 * 0);
			gfx::packFloat4x4(viewMtx, transformCache.data + 16 * 1);
			gfx::packFloat4x4(projMtx, transformCache.data + 16 * 2);
			bgfx::setTransform(transformCacheIndex);
			bgfx::setViewTransform(view.id, transformCache.data + 16 * 1, transformCache.data + 16 * 2);

			bgfx::setVertexBuffer(0, prim.vb);
			bgfx::setIndexBuffer(prim.ib);

			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);

			bgfx::submit(view.id, prog);
			frame.popView();

			frame.end();
		}
		spdlog::info("sandbox tick ended");
	}
};
static App instance;

extern "C" {
void gfx_init(const char *canvas) {
	try {
		instance.init(canvas);
	} catch (std::exception &ex) {
		spdlog::error("Exception thrown: {}", ex.what());
	}
}
void gfx_tick() {
	try {
		instance.tick();
	} catch (std::exception &ex) {
		spdlog::error("Exception thrown: {}", ex.what());
	}
}
std::string getExceptionMessage(intptr_t exceptionPtr) { return std::string(reinterpret_cast<std::exception *>(exceptionPtr)->what()); }
}

#include <emscripten/bind.h>
EMSCRIPTEN_BINDINGS(Bindings) { emscripten::function("getExceptionMessage", &getExceptionMessage); };
