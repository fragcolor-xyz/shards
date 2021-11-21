#include "draw_fixture.hpp"
#include <bgfx/bgfx.h>
#include <catch2/catch_all.hpp>
#include <gfx/context.hpp>
#include <gfx/frame.hpp>
#include <gfx/geom.hpp>
#include <gfx/linalg.hpp>
#include <gfx/material.hpp>
#include <gfx/mesh.hpp>
#include <gfx/shaderc.hpp>
#include <gfx/types.hpp>
#include <gfx/view.hpp>
#include <linalg/linalg.h>

TEST_CASE_METHOD(DrawFixture, "Clear", "[draw]") {
	gfx::Color clearColor = gfx::Color(100, 0, 200, 255);

	bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, gfx::colorToRGBA(clearColor));
	bgfx::touch(0);
	frameWithCaptureSync();
	CHECK_FRAME("clear");
}

TEST_CASE_METHOD(DrawFixture, "Draw sphere", "[draw]") {
	gfx::Color clearColor = gfx::Color(0, 0, 0, 255);

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

	shadercOutput.binary.clear();
	shadercOptions.shaderType = 'v';
	shadercOptions.setCompatibleForContext(context);
	CHECK(shaderCompiler.compile(shadercOptions, shadercOutput, vars, vsCode, sizeof(vsCode)));
	bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(shadercOutput.binary.data(), shadercOutput.binary.size()));

	shadercOutput.binary.clear();
	shadercOptions.shaderType = 'f';
	shadercOptions.setCompatibleForContext(context);
	CHECK(shaderCompiler.compile(shadercOptions, shadercOutput, vars, psCode, sizeof(psCode)));
	bgfx::ShaderHandle ps = bgfx::createShader(bgfx::copy(shadercOutput.binary.data(), shadercOutput.binary.size()));

	bgfx::ProgramHandle prog = bgfx::createProgram(vs, ps, false);

	bgfx::VertexBufferHandle vb;
	bgfx::IndexBufferHandle ib;
	generateSphereMesh(vb, ib);

	{
		gfx::FrameCaptureSync _(context);
		gfx::FrameRenderer frame(context);
		frame.begin();

		gfx::View &view = frame.pushMainOutputView();
		bgfx::setViewClear(view.id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, gfx::colorToRGBA(clearColor));

		setWorldViewProj(view);

		bgfx::setVertexBuffer(0, vb);
		bgfx::setIndexBuffer(ib);

		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);

		bgfx::submit(view.id, prog);

		frame.popView();
		frame.end(true);
	}
	CHECK_FRAME("draw");
}