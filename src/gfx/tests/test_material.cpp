#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "draw_fixture.hpp"
#include "test_data.hpp"
#include <gfx/texture.hpp>
#include <catch2/catch_all.hpp>
#include <gfx/frame.hpp>
#include <gfx/material.hpp>
#include <gfx/material_shader.hpp>
#include <gfx/texture.hpp>
#include <gfx/view_framebuffer.hpp>

using namespace gfx;

TEST_CASE("Material hash", "[material]") {
	Material matA, matB;
	CHECK(matA.getStaticHash() == matB.getStaticHash());
	CHECK(matA.getStaticHash().low != 0);
	CHECK(matA.getStaticHash().high != 0);

	matB.modify([](MaterialData &mat) {
		mat.flags = MaterialStaticFlags::Lit;
		mat.vectorParameters["baseColor"] = float4(0, 1, 0, 1);
	});
	CHECK(matA.getStaticHash() != matB.getStaticHash());

	Material matC = matB;
	CHECK(matC.getStaticHash() == matB.getStaticHash());

	auto f = [](MaterialData &mat) {
		MaterialTextureSlot &textureSlot = mat.textureSlots["tex"];
		textureSlot.texCoordIndex = 1;
	};
	matC.modify(f);
	CHECK(matC.getStaticHash() != matB.getStaticHash());

	matB.modify(f);
	CHECK(matC.getStaticHash() == matB.getStaticHash());
}

struct MaterialFixture : public DrawFixture {
	bgfx::VertexBufferHandle vb;
	bgfx::IndexBufferHandle ib;

	MaterialFixture() { generateSphereMesh(vb, ib); }

	void drawTestFrame(MaterialUsageContext &materialContext) {
		gfx::FrameCaptureSync _(context);
		gfx::FrameRenderer frame(context);
		frame.begin();

		gfx::View &view = frame.pushMainOutputView();
		bgfx::setViewClear(view.id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, gfx::colorToRGBA(Color(0, 0, 0, 255)));

		renderTestSphere(view, materialContext);

		frame.popView();
		frame.end(true);
	}

	void renderTestSphere(gfx::View &view, MaterialUsageContext &materialContext) {
		setWorldViewProj(view);

		bgfx::setVertexBuffer(0, vb);
		bgfx::setIndexBuffer(ib);

		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);

		materialContext.staticOptions.usageFlags = MaterialUsageFlags::HasNormals;

		ShaderProgramPtr shaderProgram = materialContext.compileProgram();
		CHECK(shaderProgram);
		if (shaderProgram) {
			materialContext.bindUniforms();
			bgfx::submit(view.id, shaderProgram->handle);
		}
	}
};

TEST_CASE_METHOD(MaterialFixture, "Default material", "[material]") {
	MaterialBuilderContext mbc(context);
	MaterialUsageContext materialContext(mbc);

	drawTestFrame(materialContext);
	CHECK_FRAME("materialDefault");
}

TEST_CASE_METHOD(MaterialFixture, "Default material - base color", "[material]") {
	MaterialBuilderContext mbc(context);
	MaterialUsageContext materialContext(mbc);
	materialContext.material.modify([](MaterialData &data) { data.vectorParameters[MaterialBuiltin::baseColor] = Colorf(0.0f, 1.0f, 0.0f, 1.0f); });

	drawTestFrame(materialContext);
	CHECK_FRAME("materialBaseColor");
}

TEST_CASE_METHOD(MaterialFixture, "Default material - base color texture", "[material]") {
	MaterialBuilderContext mbc(context);
	MaterialUsageContext materialContext(mbc);
	materialContext.material.modify([](MaterialData &data) {
		MaterialTextureSlot &textureSlot = data.textureSlots[MaterialBuiltin::baseColorTexture];
		textureSlot.texCoordIndex = 0;
		data.vectorParameters[MaterialBuiltin::baseColor] = Colorf(0.5f, 0.5f, 0.25f, 1.0f);
	});

	drawTestFrame(materialContext);
	CHECK_FRAME("materialBaseColorTexture");
}

TEST_CASE_METHOD(MaterialFixture, "Default material - custom pixel shader", "[material]") {
	MaterialBuilderContext mbc(context);
	MaterialUsageContext materialContext(mbc);
	materialContext.material.modify([](MaterialData &data) {
		data.pixelCode = R"(
void main(inout MaterialInfo mi) {
	float scale = 8.0;
	float grid = float((int(mi.texcoord0.x * scale) + int(mi.texcoord0.y * scale)) % 2);
	mi.color = mix(vec4(1,1,1,1), vec4(.5,.5,.5,1.0), grid);
};
)";
	});

	drawTestFrame(materialContext);
	CHECK_FRAME("materialCustomPixelShader");
}

TEST_CASE_METHOD(MaterialFixture, "Default material - custom vertex shader", "[material]") {
	MaterialBuilderContext mbc(context);
	MaterialUsageContext materialContext(mbc);
	materialContext.material.modify([](MaterialData &data) {
		data.vertexCode = R"(
void main(inout MaterialInfo mi) {
	float angle = acos(abs(mi.normal.x));
	float bump = cos(angle * 8.0) * 0.05;
	mi.localPosition += mi.normal * bump;
	mi.worldPosition = mul(mi.worldMatrix, vec4(mi.localPosition.xyz, 1.0)).xyz;
	mi.ndcPosition = mul(u_viewProj, vec4(mi.worldPosition, 1.0));
};
)";
	});

	drawTestFrame(materialContext);
	CHECK_FRAME("materialCustomVertexShader");
}

TEST_CASE_METHOD(MaterialFixture, "Material multiple render targets", "[material]") {
	MaterialBuilderContext mbc(context);
	MaterialUsageContext materialContext(mbc);
	materialContext.material.modify([](MaterialData &data) {
		data.mrtOutputs = {0, 1};
		data.pixelCode = R"(
void main(inout MaterialInfo mi) {
	mi.color0 = vec4(mi.texcoord0.xy, 0, 1);
	mi.color1 = vec4(mi.normal.xyz,  1);
};
)";
	});

	gfx::ViewFrameBuffer viewFrameBuffer = {
		{1.0f, bgfx::TextureFormat::BGRA8},
		{1.0f, bgfx::TextureFormat::BGRA8},
		{1.0f, bgfx::TextureFormat::D32F},
	};
	std::vector<TexturePtr> readbackTextures;
	std::vector<uint8_t> readbackBuffers[3];
	uint32_t readbackFrameAvailable = 0;
	int2 viewSize;

	bgfx::resetView(0);
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, gfx::colorToRGBA(Color(0, 0, 0, 255)));
	bgfx::touch(0);
	bgfx::frame(true);

	{
		gfx::FrameCaptureSync _(context);
		gfx::FrameRenderer frame(context);
		frame.begin();

		gfx::View &view = frame.pushMainOutputView();
		bgfx::setViewClear(view.id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, gfx::colorToRGBA(Color(0, 0, 0, 255)));
		bgfx::touch(view.id);

		viewSize = view.getSize();

		gfx::FrameBufferPtr frameBuffer = viewFrameBuffer.begin(view);
		bgfx::setViewFrameBuffer(view.id, frameBuffer->handle);

		renderTestSphere(view, materialContext);

		frame.popView();

		View &view1 = frame.pushMainOutputView();
		size_t readbackBufferSize = view1.getSize().x * view1.getSize().y * sizeof(uint32_t);
		for (size_t i = 0; i < 3; i++) {
			auto &readbackBuffer = readbackBuffers[i];
			readbackBuffer.resize(readbackBufferSize);
			TexturePtr texture = viewFrameBuffer.getTexture(i);
			TexturePtr readbackTexture = std::make_shared<Texture>(texture->getSize(), texture->getFormat(), BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);
			readbackTextures.push_back(readbackTexture);
			bgfx::blit(view1.id, readbackTexture->handle, 0, 0, texture->handle);
			readbackFrameAvailable = bgfx::readTexture(readbackTexture->handle, readbackBuffer.data(), 0);
		}
		frame.popView();

		frame.end(true);
		uint32_t frameNumber = bgfx::frame(true);
		CHECK(frameNumber >= readbackFrameAvailable);
	}

	TestFrame frame0(readbackBuffers[0].data(), viewSize, TestFrameFormat::BGRA8, sizeof(uint32_t) * viewSize.x, false);
	TestFrame frame1(readbackBuffers[1].data(), viewSize, TestFrameFormat::BGRA8, sizeof(uint32_t) * viewSize.x, false);
	TestFrame frame2(readbackBuffers[2].data(), viewSize, TestFrameFormat::R32F, sizeof(uint32_t) * viewSize.x, false);
	CHECK(testData.checkFrame("materialMRT0", frame0));
	CHECK(testData.checkFrame("materialMRT1", frame1));
	CHECK(testData.checkFrame("materialMRT2", frame2));

	bgfx::resetView(0);
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, gfx::colorToRGBA(Color(0, 0, 0, 255)));
	bgfx::touch(0);
	bgfx::frame(true);
}
