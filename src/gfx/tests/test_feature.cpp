#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "bx/math.h"
#include "gfx/context.hpp"
#include "gfx/drawable.hpp"
#include "gfx/enums.hpp"
#include "gfx/feature.hpp"
#include "gfx/feature_pipeline.hpp"
#include "gfx/features/base_color.hpp"
#include "gfx/features/transforms.hpp"
#include "gfx/frame.hpp"
#include "gfx/material.hpp"
#include "gfx/mesh.hpp"
#include "gfx/paths.hpp"
#include "gfx/renderer.hpp"
#include "gfx/shaderc.hpp"
#include "gfx/tests/draw_fixture.hpp"
#include "gfx/texture_file.hpp"
#include "linalg/linalg.h"
#include "spdlog/spdlog.h"
#include <bgfx/bgfx.h>
#include <catch2/catch_all.hpp>
#include <gfx/drawable.hpp>
#include <gfx/feature.hpp>
#include <gfx/mesh.hpp>
#include <memory>
#include <vector>

using namespace gfx;
using namespace std;

static const uint64_t bgfxDefaultState = BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_WRITE_MASK;

TEST_CASE("state equality & hash", "[feature]") {
	FeaturePipelineState a;
	a.set_blend(BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));

	FeaturePipelineState def;
	CHECK(def != a);
	CHECK(def.getHash() != a.getHash());

	FeaturePipelineState b;
	b.set_colorWrite(0);
	CHECK(def != b);
	CHECK(def.getHash() != b.getHash());
}

TEST_CASE("combine state", "[feature]") {
	FeaturePipelineState a;
	a.set_blend(BGFX_STATE_BLEND_ALPHA);
	a.set_culling(true);

	FeaturePipelineState b;
	b.set_colorWrite(0);

	CHECK(a.getHash() != b.getHash());
	CHECK(a != b);

	FeaturePipelineState combined = a.combine(b);
	CHECK(combined != a);
	CHECK(combined != b);

	BGFXPipelineState bgfx = combined.toBGFXState(WindingOrder::CCW);
	CHECK(bgfx.state ==
		  ((bgfxDefaultState ^ (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A)) | BGFX_STATE_FRONT_CCW | BGFX_STATE_CULL_CW | BGFX_STATE_BLEND_ALPHA));
}

struct FeatureFixture : public DrawFixture {
	MeshPtr sphereMesh;
	Renderer renderer;
	DrawQueue drawQueue;
	ViewPtr mainView;

	FeatureFixture() {
		sphereMesh = generateSphereMesh();
		mainView = std::make_shared<View>();
		mainView->view = linalg::lookat_matrix(float3(0, 2.0f, 8.0f), float3(0, 0, 0), float3(0, 1, 0));
		mainView->proj = ViewPerspectiveProjection{
			bx::toRad(45.0f),
			FovDirection::Vertical,
		};
	}

	void render(const Pipeline &pipeline) {
		renderer.reset();

		FrameCaptureSync _(context);
		FrameRenderer frame(context);
		frame.begin();
		renderer.render(frame, drawQueue, pipeline, std::vector<ViewPtr>{mainView});
		frame.end(true);
	}
};

TEST_CASE_METHOD(FeatureFixture, "transform", "[feature]") {
	auto drawable = std::make_shared<Drawable>(sphereMesh, linalg::identity);
	drawQueue.add(drawable);

	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::translation_matrix(float3(2.f, 0.0f, 0.0f))));
	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::translation_matrix(float3(0.f, 2.0f, 0.0f))));
	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::translation_matrix(float3(0.f, 0.0f, 2.0f))));

	Pipeline pipeline = {
		DrawablePass(vector<FeaturePtr>{
			gfx::features::Transform::create(),
		}),
	};

	render(pipeline);
	checkFrame("transform");
}

TEST_CASE_METHOD(FeatureFixture, "color", "[feature]") {
	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::identity));

	auto mat = std::make_shared<Material>();
	mat->modify([&](MaterialData &md) { md.basicParameters["baseColor"] = float3(1, 0, 0); });
	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::translation_matrix(float3(2.f, 0.0f, 0.0f)), mat));

	auto mat1 = std::make_shared<Material>();
	mat1->modify([&](MaterialData &md) { md.basicParameters["baseColor"] = float3(0, 1, 0); });
	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::translation_matrix(float3(0.f, 2.0f, 0.0f)), mat1));

	auto mat2 = std::make_shared<Material>();
	mat2->modify([&](MaterialData &md) { md.basicParameters["baseColor"] = float3(0, 0, 1); });
	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::translation_matrix(float3(0.f, 0.0f, 2.0f)), mat2));

	Pipeline pipeline = {
		DrawablePass(vector<FeaturePtr>{
			gfx::features::Transform::create(),
			gfx::features::BaseColor::create(),
		}),
	};

	render(pipeline);
	checkFrame("color");
}

TEST_CASE_METHOD(FeatureFixture, "color texture", "[feature]") {
	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::identity));

	TexturePtr texture = gfx::textureFromFileFloat(gfx::resolveDataPath("src/gfx/tests/assets/logo.png").c_str());

	auto mat = std::make_shared<Material>();
	mat->modify([&](MaterialData &md) {
		md.basicParameters["baseColor"] = float3(1, 1, 1);
		md.textureParameters["baseColorTexture"].texture = texture;
	});
	drawQueue.add(std::make_shared<Drawable>(sphereMesh, linalg::translation_matrix(float3(2.f, 0.0f, 0.0f)), mat));

	Pipeline pipeline = {
		DrawablePass(vector<FeaturePtr>{
			gfx::features::Transform::create(),
			gfx::features::BaseColor::create(),
		}),
	};

	render(pipeline);
	checkFrame("texture");
}
