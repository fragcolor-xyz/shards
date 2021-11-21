#include "draw_fixture.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/frame.hpp>
#include <gfx/material.hpp>
#include <gfx/material_shader.hpp>
#include <gfx/texture.hpp>

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

		setWorldViewProj(view);

		bgfx::setVertexBuffer(0, vb);
		bgfx::setIndexBuffer(ib);

		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);

		materialContext.materialUsageFlags = MaterialUsageFlags::HasNormals;

		ShaderProgramPtr shaderProgram = materialContext.compileProgram();
		CHECK(shaderProgram);
		if (shaderProgram) {
			materialContext.bindUniforms();
			bgfx::submit(view.id, shaderProgram->handle);
		}

		frame.popView();
		frame.end(true);
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
};
)";
	});

	drawTestFrame(materialContext);
	CHECK_FRAME("materialCustomPixelShader");
}
