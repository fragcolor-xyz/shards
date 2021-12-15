#pragma once

#include "drawable.hpp"
#include "feature.hpp"
#include "feature_pipeline.hpp"
#include "frame.hpp"
#include "hash.hpp"
#include "hasherxxh128.hpp"
#include "material_shader.hpp"
#include "mesh.hpp"
#include "shaderc.hpp"
#include "view.hpp"
#include <initializer_list>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gfx {
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

struct FrameRenderer;
struct Renderer {
private:
	std::shared_ptr<ShaderCompilerModule> shaderCompilerModule;
	std::set<const Feature *> framePrecomputeFeatures;

public:
	Renderer() { shaderCompilerModule = createShaderCompilerModule(); }

	void render(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const Pipeline &pipeline, const std::vector<ViewPtr> &views) {
		for (auto &view : views) {
			renderView(frame, drawQueue, pipeline, view);
		}
	}

private:
	void renderView(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const Pipeline &pipeline, ViewPtr view) {
		int2 outputSize = frame.context.getMainOutputSize();

		bgfx::ViewId viewId = frame.nextViewId();
		bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL);
		bgfx::setViewRect(viewId, 0, 0, bgfx::BackbufferRatio::Equal);

		float buffer[16 * 2];
		packFloat4x4(view->view, &buffer[0]);
		packFloat4x4(view->getProjectionMatrix(outputSize), &buffer[16]);
		bgfx::setViewTransform(viewId, &buffer[0], &buffer[16]);

		for (auto &step : pipeline.steps) {
			std::visit(
				[&](auto &&arg) {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, DrawablePass>) {
						drawablePass(frame, drawQueue, arg, view);
					}
				},
				step);
		}
	}

	struct FeatureGroup {
		std::vector<const Feature *> features;
		std::vector<DrawablePtr> drawables;
	};

	void groupByFeatures(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const DrawablePass &pass, ViewPtr view,
						 std::unordered_map<Hash128, FeatureGroup> &featureGroups) {
		for (auto &drawable : drawQueue.getDrawables()) {
			if (!drawable->mesh)
				continue;
			const Mesh &mesh = *drawable->mesh.get();

			const std::vector<FeaturePtr> *featureSources[2] = {&pass.features, nullptr};

			auto &material = drawable->material;
			const MaterialData *materialData = nullptr;
			if (material) {
				materialData = &material->getData();

				featureSources[1] = &material->customFeatures;
			}

			std::vector<const Feature *> features;
			FeatureCallbackContext filterContext{frame.context, view.get(), drawable.get()};
			for (auto &featureSource : featureSources) {
				for (auto &feature : *featureSource) {
					if (featureFilter(*feature.get(), filterContext)) {
						features.push_back(feature.get());
					}
				}
			}

			HasherXXH128 featureHasher;
			for (auto &feature : features) {
				featureHasher(feature);
			}

			Hash128 featureHash = featureHasher.getDigest();

			auto it = featureGroups.find(featureHash);
			if (it == featureGroups.end()) {
				it = featureGroups.insert(std::make_pair(featureHash, FeatureGroup())).first;
				it->second.features = features;
			}

			it->second.drawables.push_back(drawable);
		}
	}

	struct PipelineGroup {
		FeatureBindingLayout bindingLayout;
		std::vector<const Feature *> features;
		std::vector<DrawablePtr> drawables;
	};

	void groupByPipeline(const FeatureGroup &featureGroup, std::unordered_map<Hash128, PipelineGroup> &outGroups) {
		HasherXXH128<HashStaticVistor> featuresHasher;
		for (auto feature : featureGroup.features) {
			featuresHasher(*feature);
		}
		Hash128 featuresHash = featuresHasher.getDigest();

		for (auto &drawable : featureGroup.drawables) {
			HasherXXH128<HashStaticVistor> hasher;
			hasher(drawable->mesh->vertexAttributes);
			hasher(drawable->mesh->windingOrder);
			hasher(featuresHash);
			Hash128 pipelineHash = hasher.getDigest();

			auto it = outGroups.find(pipelineHash);
			if (it == outGroups.end()) {
				it = outGroups.insert(std::make_pair(pipelineHash, PipelineGroup())).first;
				PipelineGroup &group = it->second;
				group.features = featureGroup.features;
				assert(buildBindingLayout(BuildBindingLayoutParams{featureGroup.features, *drawable.get()}, group.bindingLayout));
			}

			PipelineGroup &group = it->second;
			group.drawables.push_back(drawable);
		}
	}

	void drawablePass(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const DrawablePass &pass, ViewPtr view) {
		std::unordered_map<Hash128, FeatureGroup> featureGroups;
		groupByFeatures(frame, drawQueue, pass, view, featureGroups);

		std::unordered_map<Hash128, PipelineGroup> pipelineGroups;
		for (auto &it : featureGroups) {
			groupByPipeline(it.second, pipelineGroups);
		}

		for (auto &it : pipelineGroups) {
			PipelineGroup &pipelineGroup = it.second;
			for (auto &drawable : pipelineGroup.drawables) {
			}
		}

		// 	FeaturePipelinePtr pipeline = getPipeline(frame.context, features, drawable);
		// 	bgfxSetupPipeline(*pipeline.get());

		// 	float buffer[16];
		// 	packFloat4x4(drawable->transform, buffer);
		// 	bgfx::setTransform(buffer);

		// 	for (auto &binding : pipeline->bindings) {
		// 		if (binding.texture) {
		// 			TexturePtr texture;
		// 			if (materialData) {
		// 				if (auto textureSlot = materialData->findTextureSlot(binding.name))
		// 					texture = textureSlot->texture;
		// 			}
		// 			if (!texture)
		// 				texture = binding.texture->defaultValue;

		// 			bgfx::TextureHandle handle = texture ? texture->handle : bgfx::TextureHandle(BGFX_INVALID_HANDLE);
		// 			bgfx::setTexture(binding.texture->slot, binding.handle, handle);
		// 		} else {
		// 			const void *data = nullptr;
		// 			std::vector<uint8_t> buffer1;
		// 			if (materialData) {
		// 				if (auto vecParam = materialData->findBasicParameter(binding.name)) {
		// 					packFieldVariant(*vecParam, buffer1);
		// 					data = buffer1.data();
		// 				}
		// 			}
		// 			if (!data)
		// 				data = binding.defaultValue.data();

		// 			bgfx::setUniform(binding.handle, data);
		// 		}
		// 	}

		// 	bgfx::setVertexBuffer(0, mesh.vb);
		// 	bgfx::setIndexBuffer(mesh.ib);

		// 	bgfx::submit(frame.getCurrentViewId(), pipeline->program);
		// }
	}

	void runPrecompute(const std::vector<Feature *> &featurePtrs, FeatureCallbackFrequency frequency, const FeatureCallbackContext &context) {
		for (auto featurePtr : featurePtrs) {
			for (auto &it : featurePtr->precompute) {
				if (it.frequency == frequency)
					it.callback(context);
			}
		}
	}

	bool featureFilter(const Feature &feature, const FeatureCallbackContext &context) {
		for (auto &it : feature.filters) {
			bool result = it.callback(context);
			if (it.type == FeatureFilterType::Exclusion)
				result = !result;
			if (!result)
				return false;
		}
		return true;
	}

	void generateDrawData(const std::vector<Feature *> &featurePtrs, FeatureCallbackFrequency frequency, const FeatureCallbackContext &context,
						  IDrawDataCollector &collector) {
		for (auto featurePtr : featurePtrs) {
			for (auto &it : featurePtr->drawData) {
				if (it.frequency == frequency)
					it.callback(context, collector);
			}
		}
	}

	void bgfxSetupPipeline(const FeaturePipeline &pipeline) {
		bgfx::setState(pipeline.state.state);

		if (pipeline.state.scissor) {
			const short4 &scissor = pipeline.state.scissor.value();
			bgfx::setScissor(scissor.x, scissor.y, scissor.z, scissor.w);
		}

		if (pipeline.state.stencil) {
			bgfx::setStencil(pipeline.state.stencil->front, pipeline.state.stencil->back);
		}
	}

	FeaturePipelinePtr getPipeline(Context &context, const std::vector<const Feature *> &features, DrawablePtr drawable) {
		if (!pipeline) {
			BuildPipelineParams params;
			params.drawable = drawable.get();
			params.features = features;
			params.rendererType = context.getRendererType();
			params.shaderCompiler = &shaderCompilerModule->getShaderCompiler();

			PipelineOutputDesc &output = params.outputs.emplace_back();
			output.name = "color";
			output.format = bgfx::TextureFormat::RGBA8;

			pipeline = std::make_shared<FeaturePipeline>();
			if (!buildPipeline(params, *pipeline.get()))
				throw std::runtime_error("");
		}
		return pipeline;
	}

	FeaturePipelinePtr pipeline;
};

} // namespace gfx
