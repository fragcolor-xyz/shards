#include "renderer.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "feature.hpp"
#include "hasherxxh128.hpp"
#include "mesh.hpp"
#include "params.hpp"
#include "renderer_types.hpp"
#include "shader/blocks.hpp"
#include "shader/entry_point.hpp"
#include "shader/generator.hpp"
#include "shader/uniforms.hpp"
#include "view.hpp"
#include "view_texture.hpp"
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

#define GFX_RENDERER_MAX_BUFFERED_FRAMES (2)

namespace gfx {
using shader::UniformBufferLayout;
using shader::UniformBufferLayoutBuilder;
using shader::UniformLayout;

PipelineStepPtr makeDrawablePipelineStep(RenderDrawablesStep &&step) { return std::make_shared<PipelineStep>(std::move(step)); }

struct BindingLayout {
	UniformBufferLayout uniformBuffers;
};

struct CachedPipeline {
	struct DrawGroup {
		MeshContextData *mesh;
		size_t startIndex;
		size_t numInstances;
		DrawGroup(MeshContextData *mesh, size_t startIndex, size_t numInstances) : mesh(mesh), startIndex(startIndex), numInstances(numInstances) {}
	};

	MeshFormat meshFormat;
	std::vector<const Feature *> features;
	UniformBufferLayout objectBufferLayout;

	WGPURenderPipeline pipeline = {};
	WGPUShaderModule shaderModule = {};
	WGPUPipelineLayout pipelineLayout = {};
	std::vector<WGPUBindGroupLayout> bindGroupLayouts;

	std::vector<Drawable *> drawables;
	std::vector<Drawable *> drawablesSorted;
	std::vector<DrawGroup> drawGroups;

	DynamicWGPUBufferPool instanceBufferPool;

	void resetDrawables() {
		drawables.clear();
		drawablesSorted.clear();
		drawGroups.clear();
	}

	void resetPools() { instanceBufferPool.reset(); }

	void release() {
		wgpuPipelineLayoutRelease(pipelineLayout);
		wgpuShaderModuleRelease(shaderModule);
		wgpuRenderPipelineRelease(pipeline);
		for (WGPUBindGroupLayout &layout : bindGroupLayouts) {
			wgpuBindGroupLayoutRelease(layout);
		}
	}
};
typedef std::shared_ptr<CachedPipeline> CachedPipelinePtr;

static void packDrawData(uint8_t *outData, size_t outSize, const UniformBufferLayout &layout, const DrawData &drawData) {
	size_t layoutIndex = 0;
	for (auto &fieldName : layout.fieldNames) {
		auto drawDataIt = drawData.data.find(fieldName);
		if (drawDataIt != drawData.data.end()) {
			const UniformLayout &itemLayout = layout.items[layoutIndex];
			ShaderParamType drawDataFieldType = getParamVariantType(drawDataIt->second);
			if (itemLayout.type != drawDataFieldType) {
				spdlog::warn("WEBGPU: Field type mismatch layout:{} drawData:{}", magic_enum::enum_name(itemLayout.type),
							 magic_enum::enum_name(drawDataFieldType));
				continue;
			}

			packParamVariant(outData + itemLayout.offset, outSize - itemLayout.offset, drawDataIt->second);
		}
		layoutIndex++;
	}
}

struct CachedDrawableData {
	Drawable *drawable;
	CachedPipeline *pipeline;
	Hash128 pipelineHash;
};

struct CachedViewData {
	DynamicWGPUBufferPool viewBuffers;
	~CachedViewData() {}

	void resetPools() { viewBuffers.reset(); }
};
typedef std::shared_ptr<CachedViewData> CachedViewDataPtr;

struct FrameReferences {
	std::vector<std::shared_ptr<ContextData>> contextDataReferences;
	void clear() { contextDataReferences.clear(); }
};

struct RendererImpl {
	Context &context;
	WGPUSupportedLimits deviceLimits = {};
	WGPUSupportedLimits adapterLimits = {};

	UniformBufferLayout viewBufferLayout;

	Renderer::MainOutput mainOutput;
	bool shouldUpdateMainOutputFromContext = false;

	Swappable<std::vector<std::function<void()>>, GFX_RENDERER_MAX_BUFFERED_FRAMES> postFrameQueue;
	Swappable<FrameReferences, GFX_RENDERER_MAX_BUFFERED_FRAMES> frameReferences;

	std::unordered_map<View *, CachedViewDataPtr> viewCache;
	std::unordered_map<Hash128, CachedPipelinePtr> pipelineCache;
	std::unordered_map<Drawable *, CachedDrawableData> drawableCache;

	size_t frameIndex = 0;
	const size_t maxBufferedFrames = GFX_RENDERER_MAX_BUFFERED_FRAMES;

	std::unique_ptr<ViewTexture> depthTexture = std::make_unique<ViewTexture>(WGPUTextureFormat_Depth24Plus);

	RendererImpl(Context &context) : context(context) {
		UniformBufferLayoutBuilder viewBufferLayoutBuilder;
		viewBufferLayoutBuilder.push("view", ShaderParamType::Float4x4);
		viewBufferLayoutBuilder.push("proj", ShaderParamType::Float4x4);
		viewBufferLayoutBuilder.push("invView", ShaderParamType::Float4x4);
		viewBufferLayoutBuilder.push("invProj", ShaderParamType::Float4x4);
		viewBufferLayoutBuilder.push("viewport", ShaderParamType::Float4);
		viewBufferLayout = viewBufferLayoutBuilder.finalize();

		wgpuDeviceGetLimits(context.wgpuDevice, &deviceLimits);
		wgpuAdapterGetLimits(context.wgpuAdapter, &adapterLimits);
	}

	~RendererImpl() {
		context.sync();
		swapBuffers();
	}

	size_t alignToMinUniformOffset(size_t size) const { return alignTo(size, deviceLimits.limits.minUniformBufferOffsetAlignment); }
	size_t alignToArrayBounds(size_t size, size_t elementAlign) const { return alignTo(size, elementAlign); }

	size_t alignTo(size_t size, size_t alignTo) const {
		size_t alignment = alignTo;
		if (alignment == 0)
			return size;

		size_t remainder = size % alignment;
		if (remainder > 0) {
			return size + (alignment - remainder);
		}
		return size;
	}

	void updateMainOutputFromContext() {
		mainOutput.format = context.getMainOutputFormat();
		mainOutput.view = context.getMainOutputTextureView();
		mainOutput.size = context.getMainOutputSize();
	}

	void renderView(const DrawQueue &drawQueue, ViewPtr view, const PipelineSteps &pipelineSteps) {
		if (shouldUpdateMainOutputFromContext) {
			updateMainOutputFromContext();
		}

		View *viewPtr = view.get();

		Rect viewport;
		int2 viewSize;
		if (viewPtr->viewport) {
			viewSize = viewPtr->viewport->getSize();
			viewport = viewPtr->viewport.value();
		} else {
			viewSize = mainOutput.size;
			viewport = Rect(0, 0, viewSize.x, viewSize.y);
		}

		DrawData viewDrawData;
		viewDrawData.setParam("view", viewPtr->view);
		viewDrawData.setParam("invView", linalg::inverse(viewPtr->view));
		float4x4 projMatrix = viewPtr->getProjectionMatrix(viewSize);
		viewDrawData.setParam("proj", projMatrix);
		viewDrawData.setParam("invProj", linalg::inverse(projMatrix));

		auto it = viewCache.find(viewPtr);
		if (it == viewCache.end()) {
			it = viewCache.insert(std::make_pair(viewPtr, std::make_shared<CachedViewData>())).first;
		}
		CachedViewData &viewData = *it->second.get();

		DynamicWGPUBuffer &viewBuffer = viewData.viewBuffers.allocateBuffer(viewBufferLayout.size);
		viewBuffer.resize(context.wgpuDevice, viewBufferLayout.size, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, "viewUniform");

		std::vector<uint8_t> stagingBuffer;
		stagingBuffer.resize(viewBufferLayout.size);
		packDrawData(stagingBuffer.data(), stagingBuffer.size(), viewBufferLayout, viewDrawData);
		wgpuQueueWriteBuffer(context.wgpuQueue, viewBuffer, 0, stagingBuffer.data(), stagingBuffer.size());

		for (auto &step : pipelineSteps) {
			std::visit(
				[&](auto &&arg) {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, RenderDrawablesStep>) {
						renderDrawables(drawQueue, arg, view, viewport, viewBuffer);
					}
				},
				*step.get());
		}
	}

	WGPUBuffer createTransientBuffer(size_t size, WGPUBufferUsageFlags flags, const char *label = nullptr) {
		WGPUBufferDescriptor desc = {};
		desc.size = size;
		desc.label = label;
		desc.usage = flags;
		WGPUBuffer buffer = wgpuDeviceCreateBuffer(context.wgpuDevice, &desc);
		onFrameCleanup([buffer]() { wgpuBufferRelease(buffer); });
		return buffer;
	}

	void addFrameReference(std::shared_ptr<ContextData> &&contextData) {
		frameReferences(frameIndex).contextDataReferences.emplace_back(std::move(contextData));
	}

	void onFrameCleanup(std::function<void()> &&callback) { postFrameQueue(frameIndex).emplace_back(std::move(callback)); }

	void swapBuffers() {
		frameIndex = (frameIndex + 1) % maxBufferedFrames;

		frameReferences(frameIndex).clear();
		auto &postFrameQueue = this->postFrameQueue(frameIndex);
		for (auto &cb : postFrameQueue) {
			cb();
		}
		postFrameQueue.clear();

		for (auto &pair : viewCache) {
			pair.second->resetPools();
		}

		for (auto &pair : pipelineCache) {
			pair.second->resetPools();
		}
	}

	struct Bindable {
		WGPUBuffer buffer;
		UniformBufferLayout layout;
		size_t overrideSize = ~0;
		Bindable(WGPUBuffer buffer, UniformBufferLayout layout, size_t overrideSize = ~0) : buffer(buffer), layout(layout), overrideSize(overrideSize) {}
	};

	WGPUBindGroup createBindGroup(WGPUDevice device, WGPUBindGroupLayout layout, std::vector<Bindable> bindables) {
		WGPUBindGroupDescriptor desc = {};
		desc.label = "view";
		std::vector<WGPUBindGroupEntry> entries;

		size_t counter = 0;
		for (auto &bindable : bindables) {
			WGPUBindGroupEntry &entry = entries.emplace_back(WGPUBindGroupEntry{});
			entry.binding = counter++;
			entry.buffer = bindable.buffer;
			entry.size = (bindable.overrideSize != size_t(~0)) ? bindable.overrideSize : bindable.layout.size;
		}

		desc.entries = entries.data();
		desc.entryCount = entries.size();
		desc.layout = layout;
		return wgpuDeviceCreateBindGroup(device, &desc);
	}

	void fillInstanceBuffer(DynamicWGPUBuffer &instanceBuffer, CachedPipeline &cachedPipeline, View *view) {
		size_t alignedObjectBufferSize = alignToArrayBounds(cachedPipeline.objectBufferLayout.size, cachedPipeline.objectBufferLayout.maxAlignment);
		size_t numObjects = cachedPipeline.drawables.size();
		size_t instanceBufferLength = numObjects * alignedObjectBufferSize;
		instanceBuffer.resize(context.wgpuDevice, instanceBufferLength, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "objects");

		std::vector<uint8_t> drawDataTempBuffer;
		drawDataTempBuffer.resize(instanceBufferLength);
		for (size_t i = 0; i < numObjects; i++) {
			Drawable *drawable = cachedPipeline.drawablesSorted[i];

			DrawData objectDrawData;
			objectDrawData.setParam("world", drawable->transform);

			FeatureCallbackContext callbackContext{context, view, drawable};
			for (const Feature *feature : cachedPipeline.features) {
				for (const FeatureDrawDataFunction &drawDataGenerator : feature->drawData) {
					// TODO: Catch mismatch errors here
					drawDataGenerator(callbackContext, objectDrawData);
				}
			}

			size_t bufferOffset = alignedObjectBufferSize * i;
			size_t remainingBufferLength = instanceBufferLength - bufferOffset;

			packDrawData(drawDataTempBuffer.data() + bufferOffset, remainingBufferLength, cachedPipeline.objectBufferLayout, objectDrawData);
		}

		wgpuQueueWriteBuffer(context.wgpuQueue, instanceBuffer, 0, drawDataTempBuffer.data(), drawDataTempBuffer.size());
	}

	void groupDrawables(CachedPipeline &cachedPipeline) {
		std::vector<Drawable *> &drawablesSorted = cachedPipeline.drawablesSorted;

		// Sort drawables based on mesh binding
		for (auto &drawable : cachedPipeline.drawables) {
			drawable->mesh->createContextDataConditional(context);
			addFrameReference(drawable->mesh->contextData);

			auto comparison = [](const Drawable *left, const Drawable *right) { return left->mesh->contextData < right->mesh->contextData; };
			auto it = std::upper_bound(drawablesSorted.begin(), drawablesSorted.end(), drawable, comparison);
			drawablesSorted.insert(it, drawable);
		}

		// Creates draw call ranges based on mesh binding
		if (drawablesSorted.size() > 0) {
			size_t groupStart = 0;
			MeshContextData *currentMeshData = drawablesSorted[0]->mesh->contextData.get();

			auto finishGroup = [&](size_t currentIndex) {
				size_t groupLength = currentIndex - groupStart;
				if (groupLength > 0) {
					cachedPipeline.drawGroups.emplace_back(currentMeshData, groupStart, groupLength);
				}
			};
			for (size_t i = 1; i < drawablesSorted.size(); i++) {
				MeshContextData *meshData = drawablesSorted[i]->mesh->contextData.get();
				if (meshData != currentMeshData) {
					finishGroup(i);
					groupStart = i;
					currentMeshData = meshData;
				}
			}
			finishGroup(drawablesSorted.size());
		}
	}

	void resetPipelineCacheDrawables() {
		for (auto &pair : pipelineCache) {
			pair.second->resetDrawables();
		}
	}

	void renderDrawables(const DrawQueue &drawQueue, RenderDrawablesStep &step, ViewPtr view, Rect viewport, WGPUBuffer viewBuffer) {
		WGPUDevice device = context.wgpuDevice;
		WGPUCommandEncoderDescriptor desc = {};
		WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(device, &desc);

		WGPURenderPassDescriptor passDesc = {};
		passDesc.colorAttachmentCount = 1;

		WGPURenderPassColorAttachment mainAttach = {};
		mainAttach.clearColor = WGPUColor{.r = 0.1f, .g = 0.1f, .b = 0.1f, .a = 1.0f};
		mainAttach.loadOp = WGPULoadOp_Clear;
		mainAttach.view = mainOutput.view;
		mainAttach.storeOp = WGPUStoreOp_Store;

		WGPURenderPassDepthStencilAttachment depthAttach = {};
		depthAttach.clearDepth = 1.0f;
		depthAttach.depthLoadOp = WGPULoadOp_Clear;
		depthAttach.depthStoreOp = WGPUStoreOp_Store;
		depthAttach.view = depthTexture->update(device, viewport.getSize());

		passDesc.colorAttachments = &mainAttach;
		passDesc.colorAttachmentCount = 1;
		passDesc.depthStencilAttachment = &depthAttach;

		resetPipelineCacheDrawables();
		groupByPipeline(step, drawQueue.getDrawables());

		for (auto &pair : pipelineCache) {
			CachedPipeline &cachedPipeline = *pair.second.get();
			if (!cachedPipeline.pipeline) {
				buildObjectBufferLayout(cachedPipeline);
				buildPipeline(cachedPipeline);
			}

			groupDrawables(cachedPipeline);
		}

		WGPURenderPassEncoder passEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);
		wgpuRenderPassEncoderSetViewport(passEncoder, (float)viewport.x, (float)viewport.y, (float)viewport.width, (float)viewport.height, 0.0f, 1.0f);

		for (auto &pair : pipelineCache) {
			CachedPipeline &cachedPipeline = *pair.second.get();
			size_t drawBufferLength = cachedPipeline.objectBufferLayout.size * cachedPipeline.drawables.size();

			DynamicWGPUBuffer &instanceBuffer = cachedPipeline.instanceBufferPool.allocateBuffer(drawBufferLength);
			fillInstanceBuffer(instanceBuffer, cachedPipeline, view.get());

			auto drawGroups = cachedPipeline.drawGroups;
			std::vector<Bindable> bindables = {
				Bindable(viewBuffer, viewBufferLayout),
				Bindable(instanceBuffer, cachedPipeline.objectBufferLayout, drawBufferLength),
			};
			WGPUBindGroup bindGroup = createBindGroup(device, cachedPipeline.bindGroupLayouts[0], bindables);
			onFrameCleanup([bindGroup]() { wgpuBindGroupRelease(bindGroup); });

			wgpuRenderPassEncoderSetPipeline(passEncoder, cachedPipeline.pipeline);
			wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, bindGroup, 0, nullptr);

			for (auto &drawGroup : drawGroups) {
				auto meshContextData = drawGroup.mesh;

				wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, meshContextData->vertexBuffer, 0, 0);
				if (meshContextData->indexBuffer) {
					WGPUIndexFormat indexFormat = getWGPUIndexFormat(meshContextData->format.indexFormat);
					wgpuRenderPassEncoderSetIndexBuffer(passEncoder, meshContextData->indexBuffer, indexFormat, 0, 0);

					wgpuRenderPassEncoderDrawIndexed(passEncoder, (uint32_t)meshContextData->numIndices, drawGroup.numInstances, 0, 0, drawGroup.startIndex);
				} else {
					wgpuRenderPassEncoderDraw(passEncoder, (uint32_t)meshContextData->numVertices, drawGroup.numInstances, 0, drawGroup.startIndex);
				}
			}
		}

		wgpuRenderPassEncoderEndPass(passEncoder);

		WGPUCommandBufferDescriptor cmdBufDesc = {};
		WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

		wgpuQueueSubmit(context.wgpuQueue, 1, &cmdBuf);
	}

	void groupByPipeline(RenderDrawablesStep &step, const std::vector<DrawablePtr> &drawables) {
		// TODO: Paralellize
		std::vector<const Feature *> features;
		const std::vector<FeaturePtr> *featureSources[2] = {&step.features, nullptr};
		for (auto &drawable : drawables) {
			Drawable *drawablePtr = drawable.get();
			const Mesh &mesh = *drawablePtr->mesh.get();

			features.clear();
			for (auto &featureSource : featureSources) {
				if (!featureSource)
					continue;

				for (auto &feature : *featureSource) {
					features.push_back(feature.get());
				}
			}

			HasherXXH128 featureHasher;
			for (auto &feature : features) {
				// NOTE: Hashed by pointer since features are shared/ref-counted
				featureHasher(feature);
			}
			Hash128 featureHash = featureHasher.getDigest();

			auto drawableIt = drawableCache.find(drawablePtr);
			if (drawableIt == drawableCache.end()) {
				drawableIt = drawableCache.insert(std::make_pair(drawablePtr, CachedDrawableData{})).first;
				auto &drawableCache = drawableIt->second;
				drawableCache.drawable = drawablePtr;

				HasherXXH128<HashStaticVistor> hasher;
				hasher(mesh.getFormat());
				hasher(featureHash);
				drawableCache.pipelineHash = hasher.getDigest();
			}

			Hash128 pipelineHash = drawableIt->second.pipelineHash;
			auto it1 = pipelineCache.find(pipelineHash);
			if (it1 == pipelineCache.end()) {
				it1 = pipelineCache.insert(std::make_pair(pipelineHash, std::make_shared<CachedPipeline>())).first;
				CachedPipeline &cachedPipeline = *it1->second.get();
				cachedPipeline.meshFormat = mesh.getFormat();
				cachedPipeline.features = features;
			}

			CachedPipelinePtr &pipelineGroup = it1->second;
			pipelineGroup->drawables.push_back(drawablePtr);
		}
	}

	shader::GeneratorOutput generateShader(const CachedPipeline &cachedPipeline) {
		using namespace shader;
		using namespace shader::blocks;

		shader::Generator generator;
		generator.meshFormat = cachedPipeline.meshFormat;

		FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);
		generator.outputFields.emplace_back("color", colorFieldType);

		generator.viewBufferLayout = viewBufferLayout;
		generator.objectBufferLayout = cachedPipeline.objectBufferLayout;

		std::vector<const EntryPoint *> entryPoints;
		for (auto &feature : cachedPipeline.features) {
			for (auto &entryPoint : feature->shaderEntryPoints) {
				entryPoints.push_back(&entryPoint);
			}
		}

		return generator.build(entryPoints);
	}

	void buildObjectBufferLayout(CachedPipeline &cachedPipeline) {
		UniformBufferLayoutBuilder objectBufferLayoutBuilder;
		objectBufferLayoutBuilder.push("world", ShaderParamType::Float4x4);
		for (const Feature *feature : cachedPipeline.features) {
			for (auto &param : feature->shaderParams) {
				objectBufferLayoutBuilder.push(param.name, param.type);
			}
		}

		cachedPipeline.objectBufferLayout = objectBufferLayoutBuilder.finalize();
	}

	void buildPipeline(CachedPipeline &cachedPipeline) {
		FeaturePipelineState pipelineState{};
		WGPUDevice device = context.wgpuDevice;

		shader::GeneratorOutput generatorOutput = generateShader(cachedPipeline);
		if (generatorOutput.errors.size() > 0) {
			shader::GeneratorOutput::dumpErrors(generatorOutput);
			assert(false);
		}

		WGPUShaderModuleDescriptor moduleDesc = {};
		WGPUShaderModuleWGSLDescriptor wgslModuleDesc = {};
		moduleDesc.label = "pipeline";
		moduleDesc.nextInChain = &wgslModuleDesc.chain;

		wgslModuleDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
		wgslModuleDesc.code = generatorOutput.wgslSource.c_str();
		spdlog::info("Generated WGSL:\n {}", generatorOutput.wgslSource);

		cachedPipeline.shaderModule = wgpuDeviceCreateShaderModule(context.wgpuDevice, &moduleDesc);
		assert(cachedPipeline.shaderModule);

		WGPURenderPipelineDescriptor desc = {};

		WGPUBindGroupLayout mainBindGroupLayout;
		{
			WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
			std::vector<WGPUBindGroupLayoutEntry> bindGroupLayoutEntries;
			WGPUBindGroupLayoutEntry &viewEntry = bindGroupLayoutEntries.emplace_back();
			viewEntry.binding = 0;
			viewEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
			viewEntry.buffer.type = WGPUBufferBindingType_Uniform;
			viewEntry.buffer.hasDynamicOffset = false;
			WGPUBindGroupLayoutEntry &objectEntry = bindGroupLayoutEntries.emplace_back();
			objectEntry.binding = 1;
			objectEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
			objectEntry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
			objectEntry.buffer.hasDynamicOffset = false;
			bindGroupLayoutDesc.entries = bindGroupLayoutEntries.data();
			bindGroupLayoutDesc.entryCount = bindGroupLayoutEntries.size();
			mainBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
			cachedPipeline.bindGroupLayouts.push_back(mainBindGroupLayout);
		}
		WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
		pipelineLayoutDesc.bindGroupLayouts = &mainBindGroupLayout;
		pipelineLayoutDesc.bindGroupLayoutCount = 1;
		desc.layout = cachedPipeline.pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

		WGPUVertexState &vertex = desc.vertex;

		std::vector<WGPUVertexAttribute> attributes;
		WGPUVertexBufferLayout vertexLayout = {};
		size_t vertexStride = 0;
		size_t shaderLocationCounter = 0;
		for (auto &attr : cachedPipeline.meshFormat.vertexAttributes) {
			WGPUVertexAttribute &wgpuAttribute = attributes.emplace_back();
			wgpuAttribute.offset = uint64_t(vertexStride);
			wgpuAttribute.format = getWGPUVertexFormat(attr.type, attr.numComponents);
			wgpuAttribute.shaderLocation = shaderLocationCounter++;
			size_t typeSize = getVertexAttributeTypeSize(attr.type);
			vertexStride += attr.numComponents * typeSize;
		}
		vertexLayout.arrayStride = vertexStride;
		vertexLayout.attributeCount = attributes.size();
		vertexLayout.attributes = attributes.data();
		vertexLayout.stepMode = WGPUVertexStepMode::WGPUVertexStepMode_Vertex;

		vertex.bufferCount = 1;
		vertex.buffers = &vertexLayout;
		vertex.constantCount = 0;
		vertex.entryPoint = "vertex_main";
		vertex.module = cachedPipeline.shaderModule;

		WGPUFragmentState fragmentState = {};
		fragmentState.entryPoint = "fragment_main";
		fragmentState.module = cachedPipeline.shaderModule;

		WGPUColorTargetState mainTarget = {};
		mainTarget.format = mainOutput.format;
		mainTarget.writeMask = pipelineState.colorWrite.value_or(WGPUColorWriteMask_All);

		WGPUBlendState blendState{};
		if (pipelineState.blend.has_value()) {
			blendState = pipelineState.blend.value();
			mainTarget.blend = &blendState;
		}

		WGPUDepthStencilState depthStencilState{};
		depthStencilState.format = depthTexture->getFormat();
		depthStencilState.depthWriteEnabled = pipelineState.depthWrite.value_or(true);
		depthStencilState.depthCompare = pipelineState.depthCompare.value_or(WGPUCompareFunction_Less);
		depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
		depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
		desc.depthStencil = &depthStencilState;

		fragmentState.targets = &mainTarget;
		fragmentState.targetCount = 1;
		desc.fragment = &fragmentState;

		desc.multisample.count = 1;
		desc.multisample.mask = ~0;

		desc.primitive.frontFace = cachedPipeline.meshFormat.windingOrder == WindingOrder::CCW ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
		if (pipelineState.culling.value_or(true)) {
			desc.primitive.cullMode = pipelineState.flipFrontFace.value_or(false) ? WGPUCullMode_Front : WGPUCullMode_Back;
		} else {
			desc.primitive.cullMode = WGPUCullMode_None;
		}

		switch (cachedPipeline.meshFormat.primitiveType) {
		case PrimitiveType::TriangleList:
			desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
			break;
		case PrimitiveType::TriangleStrip:
			desc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
			desc.primitive.stripIndexFormat = getWGPUIndexFormat(cachedPipeline.meshFormat.indexFormat);
			break;
		}

		cachedPipeline.pipeline = wgpuDeviceCreateRenderPipeline(device, &desc);
	}
};

Renderer::Renderer(Context &context) {
	impl = std::make_shared<RendererImpl>(context);
	if (!context.isHeadless()) {
		impl->shouldUpdateMainOutputFromContext = true;
	}
}

void Renderer::swapBuffers() { impl->swapBuffers(); }
void Renderer::render(const DrawQueue &drawQueue, ViewPtr view, const PipelineSteps &pipelineSteps) { impl->renderView(drawQueue, view, pipelineSteps); }
void Renderer::setMainOutput(const MainOutput &output) {
	impl->mainOutput = output;
	impl->shouldUpdateMainOutputFromContext = false;
}
void Renderer::cleanup() {
	Context &context = impl->context;
	impl.reset();
	impl = std::make_shared<RendererImpl>(context);
}

} // namespace gfx
