#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "gfx/drawable.hpp"
#include "gfx/feature.hpp"
#include "gfx/feature_pipeline.hpp"
#include "gfx/mesh.hpp"
#include "gfx/features/base_color.hpp"
#include "linalg/linalg.h"
#include "spdlog/spdlog.h"
#include <bgfx/bgfx.h>
#include <catch2/catch_all.hpp>
#include <gfx/drawable.hpp>
#include <gfx/feature.hpp>
#include <gfx/mesh.hpp>
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

TEST_CASE("build pipeline", "[feature]") {
	MeshPtr mesh = make_shared<Mesh>();
	mesh->vertexAttributes.emplace_back(bgfx::Attrib::Position, 3);
	mesh->vertexAttributes.emplace_back(bgfx::Attrib::Normal, 3);
	mesh->vertexAttributes.emplace_back(bgfx::Attrib::TexCoord0, 2);

	DrawablePtr drawable = make_shared<Drawable>(mesh, linalg::identity);

	vector<FeaturePtr> features = {
		gfx::features::BaseColor::create(),
	};

	BuildPipelineParams params;
	params.drawable = drawable.get();
	params.features = featuresToPointers(features);

	PipelineOutputDesc &output = params.outputs.emplace_back();
	output.format = bgfx::TextureFormat::RG11B10F;
	output.name = "color";

	Pipeline pipeline;
	CHECK(buildPipeline(params, pipeline));

	CHECK(pipeline.debug);
	spdlog::info("--------- vs:\n{}", pipeline.debug->stages[0].code);
	spdlog::info("--------- fs:\n{}", pipeline.debug->stages[1].code);
}
