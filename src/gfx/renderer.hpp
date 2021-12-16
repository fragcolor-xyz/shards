#pragma once

#include "bgfx/bgfx.h"
#include "drawable.hpp"
#include "enums.hpp"
#include "feature.hpp"
#include "feature_pipeline.hpp"
#include "fields.hpp"
#include "frame.hpp"
#include "hash.hpp"
#include "hasherxxh128.hpp"
#include "material_shader.hpp"
#include "mesh.hpp"
#include "shaderc.hpp"
#include "view.hpp"
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <variant>
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
private:
	std::shared_ptr<ShaderCompilerModule> shaderCompilerModule;
	std::set<const Feature *> framePrecomputeFeatures;

	DrawData frameDrawData;

public:
	Renderer() { shaderCompilerModule = createShaderCompilerModule(); }

	void reset() { framePrecomputeFeatures.empty(); }

	void render(gfx::FrameRenderer &frame, const DrawQueue &drawQueue, const Pipeline &pipeline, const std::vector<ViewPtr> &views) {
		std::vector<Feature *> allFeatures;
		getAllFeatures(pipeline, allFeatures);
		runPrecompute(allFeatures, FeaturePrecomputeCallbackContext{frame.context, views});

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
		FeatureBindingLayoutPtr bindingLayout;
		WindingOrder faceWindingOrder;
		std::vector<MeshVertexAttribute> vertexAttributes;
		std::vector<const Feature *> features;
		std::vector<DrawablePtr> drawables;
		DrawData sharedDrawData;
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
				group.faceWindingOrder = drawable->mesh->windingOrder;
				group.vertexAttributes = drawable->mesh->vertexAttributes;

				group.bindingLayout = std::make_shared<FeatureBindingLayout>();
				bool result = buildBindingLayout(BuildBindingLayoutParams{featureGroup.features, *drawable.get()}, *group.bindingLayout.get());
				assert(result);
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

		// Compute shared draw data for each pipeline group
		std::unordered_map<const Feature *, DrawData> featureSharedDrawData;
		for (auto &it : pipelineGroups) {
			PipelineGroup &group = it.second;
			for (const Feature *feature : group.features) {
				auto it1 = featureSharedDrawData.find(feature);
				if (it1 == featureSharedDrawData.end()) {
					it1 = featureSharedDrawData.insert(std::make_pair(feature, DrawData())).first;
					for (auto &it2 : feature->sharedDrawData)
						(it2)(FeatureCallbackContext{frame.context, view.get()}, it1->second);
				}

				group.sharedDrawData.append(it1->second);
			}
		}

		for (auto &it : pipelineGroups) {
			PipelineGroup &pipelineGroup = it.second;

			FeaturePipelinePtr pipeline = getPipeline(frame.context, *pipelineGroup.bindingLayout.get(), pipelineGroup.features, pipelineGroup.vertexAttributes,
													  pipelineGroup.faceWindingOrder);

			std::vector<uint8_t> tempFieldData;
			for (auto &drawable : pipelineGroup.drawables) {
				// Compute individual draw data combined with shared draw data
				DrawData drawData = pipelineGroup.sharedDrawData;
				generateDrawData(pipelineGroup.features, FeatureCallbackContext{frame.context, view.get(), drawable.get()}, drawData);

				if (drawable->material) {
					for (auto &it : drawable->material->getData().basicParameters) {
						drawData.data[it.first] = it.second;
					}
					for (auto &it : drawable->material->getData().textureParameters) {
						drawData.data[it.first] = it.second.texture;
					}
				}

				auto &basicBindings = pipelineGroup.bindingLayout->basicBindings;
				for (size_t i = 0; i < basicBindings.size(); i++) {
					auto &binding = basicBindings[i];
					auto it = drawData.data.find(binding.name);

					tempFieldData.clear();
					if (it != drawData.data.end()) {
						packFieldVariant(it->second, tempFieldData);
					} else {
						packFieldVariant(binding.defaultValue, tempFieldData);
					}

					bgfx::setUniform(binding.handle, tempFieldData.data());
				}

				auto &textureBindings = pipelineGroup.bindingLayout->textureBindings;
				for (size_t i = 0; i < textureBindings.size(); i++) {
					auto &binding = textureBindings[i];
					auto it = drawData.data.find(binding.name);

					TexturePtr texture;
					if (it != drawData.data.end()) {
						if (auto texturePtrPtr = std::get_if<TexturePtr>(&it->second))
							texture = *texturePtrPtr;
					} else {
						texture = binding.defaultValue;
					}

					bgfx::TextureHandle handle = texture ? texture->handle : bgfx::TextureHandle(BGFX_INVALID_HANDLE);
					bgfx::setTexture(i, binding.handle, handle);
				}

				bgfxSetupPipeline(*pipeline.get());

				float buffer[16];
				packFloat4x4(drawable->transform, buffer);
				bgfx::setTransform(buffer);

				auto mesh = drawable->mesh;
				if (!mesh)
					continue;

				bgfx::setVertexBuffer(0, mesh->vb);
				bgfx::setIndexBuffer(mesh->ib);

				bgfx::submit(frame.getCurrentViewId(), pipeline->program);
			}
		}

		// 	FeaturePipelinePtr pipeline = getPipeline(frame.context, features, drawable);
		// 	bgfxSetupPipeline(*pipeline.get());

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

	void getAllFeatures(const Pipeline &pipeline, std::vector<Feature *> &outFeatures) {
		std::set<Feature *> set;
		for (auto &step : pipeline.steps) {
			std::visit(
				[&](auto &&arg) {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, DrawablePass>) {
						for (auto &feature : arg.features) {
							if (set.find(feature.get()) == set.end()) {
								outFeatures.push_back(feature.get());
								set.insert(feature.get());
							}
						}
					}
				},
				step);
		}
	}

	void runPrecompute(const std::vector<Feature *> &featurePtrs, const FeaturePrecomputeCallbackContext &context) {
		for (auto featurePtr : featurePtrs) {
			for (auto &it : featurePtr->precompute) {
				(it)(context);
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

	void generateSharedDrawData(const std::vector<const Feature *> &featurePtrs, const FeatureCallbackContext &context, IDrawDataCollector &collector) {
		for (auto featurePtr : featurePtrs) {
			for (auto &it : featurePtr->sharedDrawData) {
				(it)(context, collector);
			}
		}
	}

	void generateDrawData(const std::vector<const Feature *> &featurePtrs, const FeatureCallbackContext &context, IDrawDataCollector &collector) {
		for (auto featurePtr : featurePtrs) {
			for (auto &it : featurePtr->drawData) {
				(it)(context, collector);
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

	std::unordered_map<Hash128, FeaturePipelinePtr> pipelineCache;
	FeaturePipelinePtr getPipeline(Context &context, const FeatureBindingLayout &bindingLayout, const std::vector<const Feature *> &features,
								   const std::vector<MeshVertexAttribute> &vertexAttributes, WindingOrder faceWindingOrder) {
		HasherXXH128<HashStaticVistor> hasher;
		hasher(vertexAttributes);
		hasher(faceWindingOrder);
		for (auto &feature : features) {
			hasher(*feature);
		}
		Hash128 hash = hasher.getDigest();

		auto it = pipelineCache.find(hash);
		if (it == pipelineCache.end()) {
			BuildPipelineParams params{bindingLayout};
			params.faceWindingOrder = faceWindingOrder;
			params.vertexAttributes = vertexAttributes;
			params.features = features;
			params.rendererType = context.getRendererType();
			params.shaderCompiler = &shaderCompilerModule->getShaderCompiler();

			PipelineOutputDesc &output = params.outputs.emplace_back();
			output.name = "color";
			output.format = bgfx::TextureFormat::RGBA8;

			FeaturePipelinePtr newPipeline = std::make_shared<FeaturePipeline>();
			if (!buildPipeline(params, *newPipeline.get()))
				throw std::runtime_error("");

			it = pipelineCache.insert(std::make_pair(hash, newPipeline)).first;
		}

		return it->second;
	}
};

} // namespace gfx
