#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "draw_fixture.hpp"
#include "gfx/context.hpp"
#include "gfx/light.hpp"
#include "gfx/material.hpp"
#include "gfx/material_shader.hpp"
#include "gfx/scene.hpp"
#include "gfx/tests/draw_fixture.hpp"
#include "gfx/texture.hpp"
#include "linalg/linalg.h"
#include "test_data.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/frame.hpp>
#include <gfx/light.hpp>
#include <gfx/material.hpp>
#include <gfx/material_shader.hpp>
#include <gfx/texture.hpp>
#include <gfx/view_framebuffer.hpp>
#include <type_traits>
#include <windef.h>

using namespace gfx;

struct SceneFixture : public DrawFixture {
	bgfx::VertexBufferHandle vb;
	bgfx::IndexBufferHandle ib;

	SceneFixture() { generateSphereMesh(vb, ib); }

	void drawSphere(View &view, MaterialUsageContext &context) {
		setWorldViewProj(view);

		bgfx::setVertexBuffer(0, vb);
		bgfx::setIndexBuffer(ib);

		context.staticOptions.usageFlags |= MaterialUsageFlags::HasNormals;

		ShaderProgramPtr shaderProgram = context.compileProgram();
		CHECK(shaderProgram);
		if (shaderProgram) {
			context.setState();
			context.bindUniforms();
			bgfx::submit(view.id, shaderProgram->handle);
		}
	}

	void drawScene(std::function<void(SceneRenderer &)> renderScene) {
		while (pollEvents()) {
			gfx::FrameCaptureSync _(context);
			FrameRenderer frame(context);
			frame.begin();

			View &mainView = frame.pushMainOutputView();
			bgfx::setViewClear(mainView.id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL);

			SceneRenderer scene(frame);
			scene.begin();
			renderScene(scene);
			scene.end();

			frame.popView();

			frame.end();
			break;
		}
	}
};

TEST_CASE_METHOD(SceneFixture, "Scene rendering - directional lights", "[scene]") {
	Material sphereMaterial;
	sphereMaterial.modify([](MaterialData &data) { data.flags |= MaterialStaticFlags::Lit; });

	drawScene([&](SceneRenderer &scene) {
		std::shared_ptr<DirectionalLight> light = std::make_shared<DirectionalLight>();
		light->color = Colorf(1, 1, 1, 1);
		light->direction = linalg::normalize(float3(-1, -1, -1));
		scene.addLight(light);

		light = std::make_shared<DirectionalLight>();
		light->color = Colorf(.1, .5, 1, 1);
		light->direction = linalg::normalize(float3(1, -0.7, -1));
		light->intensity = 0.4f;
		scene.addLight(light);

		scene.addDrawCommand(sphereMaterial, [&](View &view, MaterialUsageContext &context) { drawSphere(view, context); });
	});
	CHECK_FRAME("materialLitDirectionalLight");
}

TEST_CASE_METHOD(SceneFixture, "Scene rendering - point lights", "[scene]") {
	Material sphereMaterial;
	sphereMaterial.modify([](MaterialData &data) { data.flags |= MaterialStaticFlags::Lit; });

	drawScene([&](SceneRenderer &scene) {
		std::shared_ptr<PointLight> light = std::make_shared<PointLight>();
		light->color = Colorf(1, 1, 1, 1);
		light->position = float3(2, 1, 2);
		light->innerRadius = 0.5f;
		light->outerRadius = 10.0f;
		light->intensity = 3.0f;
		scene.addLight(light);

		light = std::make_shared<PointLight>();
		light->color = Colorf(.2, 1, .2, 1);
		light->position = float3(-2, 0, 2);
		light->innerRadius = 0.5f;
		light->outerRadius = 10.0f;
		light->intensity = 2.0f;
		scene.addLight(light);

		light = std::make_shared<PointLight>();
		light->color = Colorf(1.0, 0, .2, 1);
		light->position = float3(-2, -3, 2);
		light->innerRadius = 0.5f;
		light->outerRadius = 10.0f;
		light->intensity = 1.0f;
		scene.addLight(light);

		scene.addDrawCommand(sphereMaterial, [&](View &view, MaterialUsageContext &context) { drawSphere(view, context); });
	});
	CHECK_FRAME("materialLitPointLight");
}