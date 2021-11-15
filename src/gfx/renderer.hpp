#pragma once

#include "bgfx/bgfx.h"
#include "drawable.hpp"
#include "feature.hpp"
#include "fields.hpp"
#include "mesh.hpp"
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace gfx {

struct FeatureBindingLayout;
typedef std::shared_ptr<FeatureBindingLayout> FeatureBindingLayoutPtr;

struct FeaturePipeline;
typedef std::shared_ptr<FeaturePipeline> FeaturePipelinePtr;

class ShaderCompilerModule;

struct DrawablePass {
	std::vector<FeaturePtr> features;

	DrawablePass(const std::vector<FeaturePtr> &features = std::vector<FeaturePtr>()) : features(features) {}
	DrawablePass(const std::initializer_list<FeaturePtr> &features) : features(features) {}
};

typedef std::variant<DrawablePass> PipelineStep;

struct Pipeline {
	std::vector<PipelineStep> steps;

	Pipeline() = default;
	Pipeline(const std::initializer_list<PipelineStep> &steps) : steps(steps) {}
};

struct DrawData : IDrawDataCollector {
	std::unordered_map<std::string, FieldVariant> data;

	void set_float(const char *name, const float &v) { data[name] = v; }
	void set_vec2(const char *name, const float2 &v) { data[name] = v; }
	void set_vec3(const char *name, const float3 &v) { data[name] = v; }
	void set_vec4(const char *name, const float4 &v) { data[name] = v; }
	void set_mat4(const char *name, const float4x4 &v) { data[name] = v; }
	void set_texture2D(const char *name, const TexturePtr &v) { data[name] = v; }
	void append(const DrawData &other) {
		for (auto &it : other.data) {
			data.insert_or_assign(it.first, it.second);
		}
	}
};

struct FrameRenderer;
struct Renderer {
	struct FeatureGroup {
		std::vector<const Feature *> features;
		std::vector<DrawablePtr> drawables;
	};

	struct PipelineGroup {
		FeatureBindingLayoutPtr bindingLayout;
		WindingOrder faceWindingOrder;
		std::vector<MeshVertexAttribute> vertexAttributes;
		std::vector<const Feature *> features;
		std::vector<DrawablePtr> drawables;
		DrawData sharedDrawData;
	};

private:
	std::shared_ptr<ShaderCompilerModule> shaderCompilerModule;
	std::set<const Feature *> framePrecomputeFeatures;
	std::unordered_map<Hash128, FeaturePipelinePtr> pipelineCache;

	DrawData frameDrawData;

public:
	Renderer();

	void reset();

	void render(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const Pipeline &pipeline, const std::vector<ViewPtr> &views);

private:
	void renderView(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const Pipeline &pipeline, ViewPtr view);

	void groupByFeatures(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const DrawablePass &pass, ViewPtr view,
						 std::unordered_map<Hash128, FeatureGroup> &featureGroups);

	void groupByPipeline(const FeatureGroup &featureGroup, std::unordered_map<Hash128, PipelineGroup> &outGroups);

	void drawablePass(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const DrawablePass &pass, ViewPtr view);

	void getAllFeatures(const Pipeline &pipeline, std::vector<Feature *> &outFeatures);

	void runPrecompute(const std::vector<Feature *> &featurePtrs, const FeaturePrecomputeCallbackContext &context);

	bool featureFilter(const Feature &feature, const FeatureCallbackContext &context);

	void generateSharedDrawData(const std::vector<const Feature *> &featurePtrs, const FeatureCallbackContext &context, IDrawDataCollector &collector);

	void generateDrawData(const std::vector<const Feature *> &featurePtrs, const FeatureCallbackContext &context, IDrawDataCollector &collector);

	void bgfxSetupPipeline(const FeaturePipeline &pipeline);

	FeaturePipelinePtr getPipeline(Context &context, const FeatureBindingLayout &bindingLayout, const std::vector<const Feature *> &features,
								   const std::vector<MeshVertexAttribute> &vertexAttributes, WindingOrder faceWindingOrder);
};

} // namespace gfx
