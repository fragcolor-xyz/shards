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

struct BindingLayout {
	UniformBufferLayout uniformBuffers;
};

struct CachedPipeline {
	MeshFormat meshFormat;

	WGPURenderPipeline pipeline = {};
	WGPUShaderModule shaderModule = {};
	WGPUPipelineLayout pipelineLayout = {};
	std::vector<WGPUBindGroupLayout> bindGroupLayouts;

	std::vector<Drawable *> drawables;
	Swappable<DynamicWGPUBuffer, GFX_RENDERER_MAX_BUFFERED_FRAMES> instanceBuffers;

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
	Swappable<DynamicWGPUBuffer, GFX_RENDERER_MAX_BUFFERED_FRAMES> viewBuffers;
	~CachedViewData() {}
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
	UniformBufferLayout objectBufferLayout;

	Renderer::MainOutput mainOutput;
	bool shouldUpdateMainOutputFromContext = false;

	Swappable<std::vector<std::function<void()>>, GFX_RENDERER_MAX_BUFFERED_FRAMES> postFrameQueue;
	Swappable<FrameReferences, GFX_RENDERER_MAX_BUFFERED_FRAMES> frameReferences;

	std::unordered_map<View *, CachedViewDataPtr> viewCache;
	std::unordered_map<Hash128, CachedPipelinePtr> pipelineCache;
	std::unordered_map<Drawable *, CachedDrawableData> drawableCache;

	size_t frameIndex = 0;
	const size_t maxBufferedFrames = GFX_RENDERER_MAX_BUFFERED_FRAMES;

	std::vector<shader::EntryPoint> testShader;
	shader::Generator testShaderGenerator;

	std::unique_ptr<ViewTexture> depthTexture = std::make_unique<ViewTexture>(WGPUTextureFormat_Depth24Plus);

	RendererImpl(Context &context) : context(context) {
		UniformBufferLayoutBuilder viewBufferLayoutBuilder;
		viewBufferLayoutBuilder.push("view", ShaderParamType::Float4x4);
		viewBufferLayoutBuilder.push("proj", ShaderParamType::Float4x4);
		viewBufferLayoutBuilder.push("invView", ShaderParamType::Float4x4);
		viewBufferLayoutBuilder.push("invProj", ShaderParamType::Float4x4);
		viewBufferLayoutBuilder.push("viewport", ShaderParamType::Float4);
		viewBufferLayout = viewBufferLayoutBuilder.finalize();

		UniformBufferLayoutBuilder objectBufferLayoutBuilder;
		objectBufferLayoutBuilder.push("world", ShaderParamType::Float4x4);
		objectBufferLayout = objectBufferLayoutBuilder.finalize();

		wgpuDeviceGetLimits(context.wgpuDevice, &deviceLimits);
		wgpuAdapterGetLimits(context.wgpuAdapter, &adapterLimits);

		initTestShader();
	}

	void initTestShader() {
		using namespace shader;
		using namespace shader::blocks;

		FieldType positionFieldType(ShaderFieldBaseType::Float32, 4);
		FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);

		testShaderGenerator.interpolatedFields.emplace_back("position", FieldType(ShaderFieldBaseType::Float32, 4));
		testShaderGenerator.interpolatedFields.emplace_back("color", colorFieldType);
		testShaderGenerator.outputFields.emplace_back("color", colorFieldType);

		auto vec4Pos = makeCompoundBlock("vec4<f32>(", ReadInput("position"), ".xyz, 1.0)");

		testShader.emplace_back("initWorldPosition", ProgrammableGraphicsStage::Vertex,
								WriteGlobal("worldPosition", positionFieldType, ReadBuffer("object"), ".world", "*", vec4Pos->clone()));
		auto &initScreenPosition = testShader.emplace_back(
			"initScreenPosition", ProgrammableGraphicsStage::Vertex,
			WriteGlobal("screenPosition", positionFieldType, ReadBuffer("view"), ".proj", "*", ReadBuffer("view"), ".view", "*", ReadGlobal("worldPosition")));
		initScreenPosition.dependencies.emplace_back("initWorldPosition");

		auto &writePosition =
			testShader.emplace_back("writePosition", ProgrammableGraphicsStage::Vertex, WriteOutput("position", ReadGlobal("screenPosition")));
		writePosition.dependencies.emplace_back("initScreenPosition");

		testShader.emplace_back("initColor", ProgrammableGraphicsStage::Vertex,
								WriteGlobal("color", colorFieldType, WithInput("color", ReadInput("color"), "vec4<f32>(1.0, 0.0, 1.0, 1.0)")));
		auto &writeColor = testShader.emplace_back("writeColor", ProgrammableGraphicsStage::Vertex, WriteOutput("color", ReadGlobal("color")));
		writeColor.dependencies.emplace_back("initColor");

		auto &normalColor = testShader.emplace_back("normalColor", ProgrammableGraphicsStage::Vertex,
													WithInput("normal", WriteGlobal("color", colorFieldType, "vec4<f32>(", ReadInput("normal"), ".xyz, 1.0)")));
		normalColor.dependencies.emplace_back("initColor");
		normalColor.dependencies.emplace_back("writeColor", DependencyType::Before);

		EntryPoint &applyVertexColor = testShader.emplace_back(
			"applyVertexColor", ProgrammableGraphicsStage::Vertex,
			WithInput("color", WriteGlobal("color", colorFieldType, makeCompoundBlock(ReadGlobal("color"), "*", ReadInput("color"), ";"))));
		applyVertexColor.dependencies.emplace_back("initColor", DependencyType::After);
		applyVertexColor.dependencies.emplace_back("writeColor", DependencyType::Before);

		testShader.emplace_back("color", ProgrammableGraphicsStage::Fragment, WithInput("color", WriteOutput("color", ReadInput("color"))));

		testShaderGenerator.viewBufferLayout = viewBufferLayout;
		testShaderGenerator.objectBufferLayout = objectBufferLayout;
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

	void renderView(const DrawQueue &drawQueue, ViewPtr view) {
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
		viewDrawData.set_float4x4("view", viewPtr->view);
		viewDrawData.set_float4x4("invView", linalg::inverse(viewPtr->view));
		float4x4 projMatrix = viewPtr->getProjectionMatrix(viewSize);
		viewDrawData.set_float4x4("proj", projMatrix);
		viewDrawData.set_float4x4("invProj", linalg::inverse(projMatrix));

		auto it = viewCache.find(viewPtr);
		if (it == viewCache.end()) {
			it = viewCache.insert(std::make_pair(viewPtr, std::make_shared<CachedViewData>())).first;
		}
		CachedViewData &viewData = *it->second.get();

		DynamicWGPUBuffer &viewBuffer = viewData.viewBuffers(frameIndex);
		viewBuffer.resize(context.wgpuDevice, viewBufferLayout.size, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, "viewUniform");

		std::vector<uint8_t> stagingBuffer;
		stagingBuffer.resize(viewBufferLayout.size);
		packDrawData(stagingBuffer.data(), stagingBuffer.size(), viewBufferLayout, viewDrawData);
		wgpuQueueWriteBuffer(context.wgpuQueue, viewBuffer, 0, stagingBuffer.data(), stagingBuffer.size());

		renderDrawablePass(drawQueue, view, viewport, viewBuffer);
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

	void renderDrawablePass(const DrawQueue &drawQueue, ViewPtr view, Rect viewport, WGPUBuffer viewBuffer) {
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

		clearPipelineCacheDrawables();
		groupByPipeline(drawQueue.getDrawables());
		for (auto &pair : pipelineCache) {
			CachedPipelinePtr &pipeline = pair.second;

			size_t alignedObjectBufferSize = alignToArrayBounds(objectBufferLayout.size, objectBufferLayout.maxAlignment);
			size_t numObjects = pipeline->drawables.size();
			size_t instanceBufferLength = numObjects * alignedObjectBufferSize;
			DynamicWGPUBuffer &instanceBuffer = pipeline->instanceBuffers(frameIndex);
			instanceBuffer.resize(device, instanceBufferLength, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, "objects");

			std::vector<uint8_t> drawDataTempBuffer;
			drawDataTempBuffer.resize(instanceBufferLength);
			for (size_t i = 0; i < numObjects; i++) {
				Drawable *drawable = pipeline->drawables[i];
				drawable->mesh->createContextDataConditional(context);
				addFrameReference(drawable->mesh->contextData);

				DrawData worldDrawData;
				worldDrawData.set_float4x4("world", drawable->transform);

				size_t bufferOffset = alignedObjectBufferSize * i;
				size_t remainingBufferLength = instanceBufferLength - bufferOffset;
				packDrawData(drawDataTempBuffer.data() + bufferOffset, remainingBufferLength, objectBufferLayout, worldDrawData);
			}

			wgpuQueueWriteBuffer(context.wgpuQueue, instanceBuffer, 0, drawDataTempBuffer.data(), drawDataTempBuffer.size());
		}

		WGPURenderPassEncoder passEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);
		wgpuRenderPassEncoderSetViewport(passEncoder, (float)viewport.x, (float)viewport.y, (float)viewport.width, (float)viewport.height, 0.0f, 1.0f);

		for (auto &pair : pipelineCache) {
			CachedPipelinePtr &pipeline = pair.second;
			std::vector<Drawable *> drawables = pipeline->drawables;

			if (!pipeline->pipeline) {
				buildPipeline(pipeline);
			}
			auto it = pipelineCache.find(pair.first);
			if (it != pipelineCache.end()) {
				pipeline = it->second;
			} else {
				pipelineCache.insert_or_assign(pair.first, pipeline);
			}

			size_t drawBufferLength = objectBufferLayout.size * pipeline->drawables.size();
			std::vector<Bindable> bindables = {
				Bindable(viewBuffer, viewBufferLayout),
				Bindable(pipeline->instanceBuffers(frameIndex), objectBufferLayout, drawBufferLength),
			};
			WGPUBindGroup bindGroup = createBindGroup(device, pipeline->bindGroupLayouts[0], bindables);
			onFrameCleanup([bindGroup]() { wgpuBindGroupRelease(bindGroup); });

			wgpuRenderPassEncoderSetPipeline(passEncoder, pipeline->pipeline);
			wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, bindGroup, 0, nullptr);

			WGPUBuffer lastVertexBuffer = nullptr;
			WGPUBuffer lastIndexBuffer = nullptr;
			for (size_t i = 0; i < drawables.size(); i++) {
				auto &drawable = drawables[i];
				auto &mesh = drawable->mesh;
				auto meshContextData = drawable->mesh->contextData;

				if (lastVertexBuffer != meshContextData->vertexBuffer) {
					wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, meshContextData->vertexBuffer, 0, 0);
					lastVertexBuffer = meshContextData->vertexBuffer;
				}

				if (meshContextData->indexBuffer) {
					if (lastIndexBuffer != meshContextData->indexBuffer) {
						wgpuRenderPassEncoderSetIndexBuffer(passEncoder, meshContextData->indexBuffer, getWGPUIndexFormat(mesh->getFormat().indexFormat), 0, 0);
						lastIndexBuffer = meshContextData->indexBuffer;
					}
					wgpuRenderPassEncoderDrawIndexed(passEncoder, (uint32_t)mesh->getNumIndices(), drawables.size(), 0, 0, 0);
					break;
				} else {
					wgpuRenderPassEncoderDraw(passEncoder, (uint32_t)mesh->getNumVertices(), 1, 0, 0);
				}
			}
		}

		wgpuRenderPassEncoderEndPass(passEncoder);

		WGPUCommandBufferDescriptor cmdBufDesc = {};
		WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

		wgpuQueueSubmit(context.wgpuQueue, 1, &cmdBuf);
	}

	void clearPipelineCacheDrawables() {
		for (auto &pair : pipelineCache) {
			pair.second->drawables.clear();
		}
	}

	void groupByPipeline(const std::vector<DrawablePtr> &drawables) {
		for (auto &drawable : drawables) {
			Drawable *drawablePtr = drawable.get();

			auto drawableIt = drawableCache.find(drawablePtr);
			if (drawableIt == drawableCache.end()) {
				drawableIt = drawableCache.insert(std::make_pair(drawablePtr, CachedDrawableData{})).first;
				auto drawableCache = drawableIt->second;
				drawableCache.drawable = drawablePtr;

				HasherXXH128<HashStaticVistor> hasher;
				hasher(drawablePtr->mesh->getFormat());
				drawableCache.pipelineHash = hasher.getDigest();
			}

			Hash128 pipelineHash = drawableIt->second.pipelineHash;
			auto it1 = pipelineCache.find(pipelineHash);
			if (it1 == pipelineCache.end()) {
				it1 = pipelineCache.insert(std::make_pair(pipelineHash, std::make_shared<CachedPipeline>())).first;
				CachedPipelinePtr &pipelineGroup = it1->second;
				pipelineGroup->meshFormat = drawablePtr->mesh->getFormat();
			}

			CachedPipelinePtr &pipelineGroup = it1->second;
			pipelineGroup->drawables.push_back(drawablePtr);
		}
	}

	void buildPipeline(const CachedPipelinePtr &result) {
		FeaturePipelineState pipelineState{};
		WGPUDevice device = context.wgpuDevice;

		shader::Generator generator = testShaderGenerator;
		generator.meshFormat = result->meshFormat;
		shader::GeneratorOutput generatorOutput = generator.build(testShader);

		WGPUShaderModuleDescriptor moduleDesc = {};
		WGPUShaderModuleWGSLDescriptor wgslModuleDesc = {};
		moduleDesc.label = "pipeline";
		moduleDesc.nextInChain = &wgslModuleDesc.chain;

		wgslModuleDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
		wgslModuleDesc.code = generatorOutput.wgslSource.c_str();
		spdlog::info("Generated WGSL:\n {}", generatorOutput.wgslSource);

		result->shaderModule = wgpuDeviceCreateShaderModule(context.wgpuDevice, &moduleDesc);
		assert(result->shaderModule);

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
			result->bindGroupLayouts.push_back(mainBindGroupLayout);
		}
		WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
		pipelineLayoutDesc.bindGroupLayouts = &mainBindGroupLayout;
		pipelineLayoutDesc.bindGroupLayoutCount = 1;
		desc.layout = result->pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

		WGPUVertexState &vertex = desc.vertex;

		std::vector<WGPUVertexAttribute> attributes;
		WGPUVertexBufferLayout vertexLayout = {};
		size_t vertexStride = 0;
		size_t shaderLocationCounter = 0;
		for (auto &attr : result->meshFormat.vertexAttributes) {
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
		vertex.module = result->shaderModule;

		WGPUFragmentState fragmentState = {};
		fragmentState.entryPoint = "fragment_main";
		fragmentState.module = result->shaderModule;

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

		desc.primitive.frontFace = result->meshFormat.windingOrder == WindingOrder::CCW ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
		if (pipelineState.culling.value_or(true)) {
			desc.primitive.cullMode = pipelineState.flipFrontFace.value_or(false) ? WGPUCullMode_Front : WGPUCullMode_Back;
		} else {
			desc.primitive.cullMode = WGPUCullMode_None;
		}

		switch (result->meshFormat.primitiveType) {
		case PrimitiveType::TriangleList:
			desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
			break;
		case PrimitiveType::TriangleStrip:
			desc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
			desc.primitive.stripIndexFormat = getWGPUIndexFormat(result->meshFormat.indexFormat);
			break;
		}

		result->pipeline = wgpuDeviceCreateRenderPipeline(device, &desc);
	}
};

Renderer::Renderer(Context &context) {
	impl = std::make_shared<RendererImpl>(context);
	if (!context.isHeadless()) {
		impl->shouldUpdateMainOutputFromContext = true;
	}
}

void Renderer::swapBuffers() { impl->swapBuffers(); }
void Renderer::render(const DrawQueue &drawQueue, ViewPtr view) { impl->renderView(drawQueue, view); }
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
