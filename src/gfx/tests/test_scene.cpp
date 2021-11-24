#include "draw_fixture.hpp"
#include "test_data.hpp"
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <catch2/catch_all.hpp>
#include <gfx/context.hpp>
#include <gfx/env_probe.hpp>
#include <gfx/frame.hpp>
#include <gfx/light.hpp>
#include <gfx/material.hpp>
#include <gfx/material_shader.hpp>
#include <gfx/math.hpp>
#include <gfx/paths.hpp>
#include <gfx/scene.hpp>
#include <gfx/texture.hpp>
#include <gfx/texture_file.hpp>
#include <gfx/view_framebuffer.hpp>
#include <linalg/linalg.h>
#include <type_traits>

using namespace gfx;

struct SceneFixture : public DrawFixture {
	bgfx::VertexBufferHandle vb;
	bgfx::IndexBufferHandle ib;
	gfx::float4x4 viewMtx = linalg::translation_matrix(gfx::float3(0, 0, -5));

	SceneFixture() { generateSphereMesh(vb, ib); }

	void setWorldTransform() {
		bgfx::Transform transformCache;

		gfx::float4x4 modelMtx = linalg::identity;

		uint32_t transformCacheIndex = bgfx::allocTransform(&transformCache, 1);
		gfx::packFloat4x4(modelMtx, transformCache.data + 16 * 0);
		bgfx::setTransform(transformCacheIndex);
	}

	void setViewProjTransform(gfx::View &view, gfx::float3 cameraLocation = gfx::float3(0, 0, -5)) {
		bgfx::Transform transformCache;

		gfx::float4x4 projMtx = linalg::perspective_matrix(pi * 0.4f, float(view.getSize().x) / view.getSize().y, 0.5f, 500.0f);

		uint32_t transformCacheIndex = bgfx::allocTransform(&transformCache, 2);
		gfx::packFloat4x4(viewMtx, transformCache.data + 16 * 0);
		gfx::packFloat4x4(projMtx, transformCache.data + 16 * 1);
		bgfx::setTransform(transformCacheIndex);
		bgfx::setViewTransform(view.id, transformCache.data + 16 * 0, transformCache.data + 16 * 1);
	}

	void drawSphere(View &view, MaterialUsageContext &context, ShaderProgramPtr program) {
		setWorldTransform();

		bgfx::setVertexBuffer(0, vb);
		bgfx::setIndexBuffer(ib);

		CHECK(program);
		if (program) {
			context.setState();
			context.bindUniforms();
			bgfx::submit(view.id, program->handle);
		}
	}

	void drawScene(std::function<void(SceneRenderer &)> renderScene) {
		while (pollEvents()) {
			gfx::FrameCaptureSync _(context);
			FrameRenderer frame(context);
			frame.begin();

			SceneRenderer scene(frame);
			scene.reset();
			renderScene(scene);
			scene.runPrecompute();

			{
				View &mainView = frame.pushMainOutputView();
				setViewProjTransform(mainView);
				bgfx::setViewClear(mainView.id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL);
				scene.renderView();
				frame.popView();
			}

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

		scene.addDrawCommand(sphereMaterial, MaterialUsageFlags::HasNormals, [&](View &view, MaterialUsageContext &context, ShaderProgramPtr program) { drawSphere(view, context, program); });
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

		scene.addDrawCommand(sphereMaterial, MaterialUsageFlags::HasNormals, [&](View &view, MaterialUsageContext &context, ShaderProgramPtr program) { drawSphere(view, context, program); });
	});
	CHECK_FRAME("materialLitPointLight");
}

TEST_CASE_METHOD(SceneFixture, "Load environment texture", "[env]") {
	std::string path = resolveDataPath("src/gfx/tests/studio_small_08.exr");
	TexturePtr texture = textureFromFileFloat(path.c_str());
	CHECK(texture);
}

TEST_CASE_METHOD(SceneFixture, "Scene rendering - environment texture", "[env]") {
	std::string path = resolveDataPath("src/gfx/tests/studio_small_08.exr");
	TexturePtr texture = textureFromFileFloat(path.c_str());
	CHECK(texture);

	Material skyMaterial;
	skyMaterial.modify([&](MaterialData &data) {
		data.textureSlots["skyTexture"].texture = texture;
		data.vertexCode = R"(
void main(inout MaterialInfo mi) {
	mi.localPosition *= 100.0f;
	mi.worldPosition = mi.localPosition + getCameraPosition();
	mi.ndcPosition = mul(u_viewProj, vec4(mi.worldPosition, 1.0));
}
)";
		data.pixelCode = R"(
void main(inout MaterialInfo mi) {
	vec3 dir = normalize(mi.worldPosition - getCameraPosition());
	mi.color = longLatTextureLod(u_skyTexture, dir, 0);
}
)";
	});

	Material sphereMaterial;
	sphereMaterial.modify([](MaterialData &data) { data.flags |= MaterialStaticFlags::Lit; });

	auto scene = [&](SceneRenderer &scene) {
		scene.addDrawCommand(skyMaterial, MaterialUsageFlags::None, [&](View &view, MaterialUsageContext &context, ShaderProgramPtr program) { drawSphere(view, context, program); });
	};
	drawScene(scene);
	CHECK_FRAME("materialEnvironmentTexture0");

	viewMtx = linalg::mul(linalg::translation_matrix(gfx::float3(0, 0, 0)), linalg::rotation_matrix(linalg::rotation_quat(float3(0, 1, 0), halfPi)));
	drawScene(scene);
	CHECK_FRAME("materialEnvironmentTexture1");

	viewMtx = linalg::mul(linalg::translation_matrix(gfx::float3(0, 0, 0)), linalg::rotation_matrix(linalg::rotation_quat(float3(0, 1, 0), -halfPi)));
	drawScene(scene);
	CHECK_FRAME("materialEnvironmentTexture2");

	viewMtx = linalg::mul(linalg::translation_matrix(gfx::float3(2000, -100, 100)), linalg::rotation_matrix(linalg::rotation_quat(float3(0, 1, 0), -halfPi)));
	drawScene(scene);
	CHECK_FRAME("materialEnvironmentTexture3");
}

TEST_CASE_METHOD(SceneFixture, "Scene rendering - environment probe", "[env]") {
	std::string path = resolveDataPath("src/gfx/tests/studio_small_08.exr");
	TexturePtr texture = textureFromFileFloat(path.c_str());
	CHECK(texture);

	Material skyMaterial;
	skyMaterial.modify([&](MaterialData &data) {
		data.textureSlots["skyTexture"].texture = texture;
	data.vertexCode = R"(
void main(inout MaterialInfo mi) {
	mi.localPosition *= 100.0f;
	mi.worldPosition = mi.localPosition + getCameraPosition();
	mi.ndcPosition = mul(u_viewProj, vec4(mi.worldPosition, 1.0));
}
)";
		data.pixelCode = R"(
void main(inout MaterialInfo mi) {
	vec3 dir = normalize(mi.worldPosition - getCameraPosition());
	dir.xz = vec2(dir.x, -dir.z);
	mi.color = longLatTextureLod(u_skyTexture, dir, 0);
}
)";
	});

	Material sphereMaterial;
	sphereMaterial.modify([](MaterialData &data) {
		data.flags |= MaterialStaticFlags::Lit;
		data.vectorParameters[MaterialBuiltin::metallic] = float4(1.0f);
		data.vectorParameters[MaterialBuiltin::roughness] = float4(0.15f);
	});

	auto scene = [&](SceneRenderer &scene) {
		scene.addProbe(std::make_shared<EnvironmentProbe>());
		scene.addDrawCommand(skyMaterial, MaterialUsageFlags::None, [&](View &view, MaterialUsageContext &context, ShaderProgramPtr program) { drawSphere(view, context, program); });
		scene.addDrawCommand(sphereMaterial, MaterialUsageFlags::HasNormals, [&](View &view, MaterialUsageContext &context, ShaderProgramPtr program) { drawSphere(view, context, program); });
	};
	drawScene(scene);
	CHECK_FRAME("materialEnvironmentProbe0");
}
