#include "draw_fixture.hpp"
#include "test_data.hpp"
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <catch2/catch_all.hpp>
#include <gfx/context.hpp>
#include <gfx/env_probe.hpp>
#include <gfx/frame.hpp>
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
			context.bindUniforms(program);
			bgfx::submit(view.id, program->handle);
		}
	}

	void drawScene(std::function<void(SceneRenderer &)> renderScene) {
		while (pollEvents()) {
			gfx::FrameCaptureSync _(context);
			FrameRenderer frame(context);
			frame.begin();

			SceneRenderer scene(context);
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
