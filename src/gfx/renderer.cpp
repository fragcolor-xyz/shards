#include "renderer.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "hasherxxh128.hpp"
#include "mesh.hpp"
#include "view.hpp"
#include <spdlog/spdlog.h>

namespace gfx {

struct UniformLayout {
	size_t offset;
	size_t size;
};

struct UniformBufferLayout {
	std::unordered_map<std::string, size_t> mapping;
	std::vector<UniformLayout> items;
	size_t size = ~0;

	UniformBufferLayout() = default;
	UniformBufferLayout(const UniformBufferLayout &other) = default;
	UniformBufferLayout(UniformBufferLayout &&other) {
		mapping = std::move(other.mapping);
		items = std::move(other.items);
		size = std::move(other.size);
	}

	UniformBufferLayout &operator=(const UniformBufferLayout &&other) {
		mapping = std::move(other.mapping);
		items = std::move(other.items);
		size = std::move(other.size);
		return *this;
	}

	UniformLayout &push(const std::string &name, UniformLayout &&layout) {
		assert(mapping.find(name) == mapping.end());
		size_t index = items.size();
		items.emplace_back(layout);
		mapping.insert_or_assign(name, index);
		return items.back();
	}
};

struct UniformBufferLayoutBuilder {
	UniformBufferLayout bufferLayout;
	size_t offset = 0;

	UniformLayout generateNext(size_t size) const {
		UniformLayout result;
		result.offset = offset;
		result.size = size;
		return result;
	}

	const UniformLayout &push(const std::string &name, UniformLayout &&layout) {
		const UniformLayout &result = bufferLayout.push(name, std::move(layout));
		offset = std::max(offset, layout.offset + layout.size);
		return result;
	}

	const UniformLayout &push(const std::string &name, size_t size) { return push(name, generateNext(size)); }

	UniformBufferLayout &&finalize() {
		bufferLayout.size = offset;
		return std::move(bufferLayout);
	}

	void fill();
};

struct BindingLayout {
	UniformBufferLayout uniformBuffers;
};

struct PipelineInternal {
	WGPURenderPipeline pipeline = {};
	WGPUShaderModule shaderModule = {};
	std::vector<WGPUBindGroupLayout> bindGroupLayouts;
	WGPUPipelineLayout pipelineLayout = {};
};
typedef std::shared_ptr<PipelineInternal> PipelineInternalPtr;

struct PipelineGroup {
	Mesh::Format meshFormat;
	// BindingLayout bindingLayout;
	std::vector<DrawablePtr> drawables;
};

struct RendererImpl {
	Context &context;

	UniformBufferLayout viewBufferLayout;
	UniformBufferLayout objectBufferLayout;

	RendererImpl(Context &context) : context(context) {
		UniformBufferLayoutBuilder viewBufferLayoutBuilder;
		viewBufferLayoutBuilder.push("view", sizeof(float) * 16);
		viewBufferLayoutBuilder.push("proj", sizeof(float) * 16);
		viewBufferLayoutBuilder.push("invView", sizeof(float) * 16);
		viewBufferLayoutBuilder.push("invProj", sizeof(float) * 16);
		viewBufferLayout = viewBufferLayoutBuilder.finalize();

		UniformBufferLayoutBuilder objectBufferLayoutBuilder;
		objectBufferLayoutBuilder.push("transform", sizeof(float) * 16);
		objectBufferLayout = objectBufferLayoutBuilder.finalize();
	}

	void renderView(const DrawQueue &drawQueue, ViewPtr view) {
		WGPUBuffer viewBuffer;

		renderDrawablePass(drawQueue, view);
	}

	WGPUBindGroup createBindGroup(WGPUDevice device, std::vector<WGPUBuffer> buffers) {
		WGPUBindGroupDescriptor desc = {};
		desc.label = "view";
		std::vector<WGPUBindGroupEntry> entries;

		size_t counter = 0;
		for (auto &buffer : buffers) {
			WGPUBindGroupEntry &entry = entries.emplace_back(WGPUBindGroupEntry{});
			entry.binding = counter++;
			entry.buffer = buffer;
		}

		desc.entries = entries.data();
		desc.entryCount = entries.size();
		wgpuDeviceCreateBindGroup(device, &desc);
	}

	void renderDrawablePass(const DrawQueue &drawQueue, ViewPtr view) {
		WGPUDevice device = context.wgpuDevice;
		WGPUCommandEncoderDescriptor desc = {};
		WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(device, &desc);

		WGPURenderPassDescriptor passDesc = {};
		passDesc.colorAttachmentCount = 1;
		WGPURenderPassColorAttachment mainAttach = {};
		mainAttach.clearColor = WGPUColor{.r = 1.0f, .g = 0.0f, .b = 1.0f, .a = 1.0f};
		mainAttach.loadOp = WGPULoadOp_Clear;
		mainAttach.view = wgpuSwapChainGetCurrentTextureView(context.wgpuSwapchain);
		mainAttach.storeOp = WGPUStoreOp_Store;
		passDesc.colorAttachments = &mainAttach;
		passDesc.colorAttachmentCount = 1;

		WGPURenderPassEncoder passEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);

		std::unordered_map<Hash128, PipelineGroup> groups;
		groupByPipeline(drawQueue.getDrawables(), groups);

		for (auto &pair : groups) {
			PipelineGroup &group = pair.second;
			PipelineInternalPtr pipeline = buildPipeline(group);

			// createBindGroup();

			for (auto &drawable : group.drawables) {
				wgpuRenderPassEncoderSetPipeline(passEncoder, pipeline->pipeline);
				// wgpuRenderPassEncoderDraw(passEncoder, group.
			}
		}

		wgpuRenderPassEncoderEndPass(passEncoder);

		WGPUCommandBufferDescriptor cmdBufDesc = {};
		WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

		wgpuQueueSubmit(context.wgpuQueue, 1, &cmdBuf);
	}

	void groupByPipeline(const std::vector<DrawablePtr> &drawables, std::unordered_map<Hash128, PipelineGroup> &outGroups) {
		for (auto &drawable : drawables) {
			HasherXXH128<HashStaticVistor> hasher;
			hasher(drawable->mesh->getFormat());
			Hash128 pipelineHash = hasher.getDigest();

			auto it = outGroups.find(pipelineHash);
			if (it == outGroups.end()) {
				it = outGroups.insert(std::make_pair(pipelineHash, PipelineGroup())).first;
				PipelineGroup &group = it->second;
				group.meshFormat = drawable->mesh->getFormat();
				// group.bindingLayout = buildBindingLayout();
			}

			PipelineGroup &group = it->second;
			group.drawables.push_back(drawable);
		}
	}

	// BindingLayout buildBindingLayout() {
	// 	UniformBufferLayoutBuilder viewBuilder;
	// 	viewBuilder.push("view", sizeof(float) * 16);
	// 	viewBuilder.push("projection", sizeof(float) * 16);

	// 	return BindingLayout{
	// 		.uniformBuffers = {viewBuilder.finalize()},
	// 	};
	// }

	std::shared_ptr<PipelineInternal> buildPipeline(PipelineGroup &group) {
		WGPUDevice device = context.wgpuDevice;

		std::shared_ptr<PipelineInternal> result = std::make_shared<PipelineInternal>();

		WGPUShaderModuleDescriptor moduleDesc = {};
		WGPUShaderModuleWGSLDescriptor wgslModuleDesc = {};
		wgslModuleDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
		moduleDesc.label = "pipeline";
		wgslModuleDesc.code = R"(
			struct VertToFrag {
				[[builtin(position)]] position: vec4<f32>;
				[[location(0)]] normal: vec3<f32>;
			};

			[[stage(vertex)]]
			fn vs_main([[location(0)]] position: vec4<f32>, [[location(1)]] normal: vec3<f32>) -> VertToFrag {
				var v2f: VertToFrag;
				v2f.position = position;
				v2f.normal = normal;
				return v2f;
			}

			[[stage(fragment)]]
			fn fs_main(v2f: VertToFrag) -> [[location(0)]] vec4<f32> {
				return vec4<f32>(1.0, 1.0, 1.0, 1.0);
				// let absNormal = abs(v2f.normal);
				// return vec4<f32>(absNormal.x, absNormal.y, absNormal.z, 1.0);
			}
		)";
		moduleDesc.nextInChain = &wgslModuleDesc.chain;
		auto errorScope = context.pushErrorScope();
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
		WGPUVertexBufferLayout vertexLayout;
		size_t vertexStride = 0;
		size_t shaderLocationCounter = 0;
		for (auto &attr : group.meshFormat.vertexAttributes) {
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
		vertex.entryPoint = "vs_main";
		vertex.module = result->shaderModule;

		WGPUFragmentState fragmentState = {};
		fragmentState.entryPoint = "fs_main";
		fragmentState.module = result->shaderModule;
		WGPUColorTargetState mainTarget = {};
		mainTarget.format = context.swapchainFormat;
		mainTarget.writeMask = WGPUColorWriteMask_All;
		fragmentState.targets = &mainTarget;
		fragmentState.targetCount = 1;
		desc.fragment = &fragmentState;

		desc.multisample.count = 1;
		desc.multisample.mask = ~0;

		result->pipeline = wgpuDeviceCreateRenderPipeline(device, &desc);
		errorScope.pop([](WGPUErrorType type, char const *message) { spdlog::error("Failed to create pipeline({}): {}", type, message); });
		return result;
	};
};

Renderer::Renderer(Context &context) { impl = std::make_shared<RendererImpl>(context); }

void Renderer::render(const DrawQueue &drawQueue, ViewPtr view) { impl->renderView(drawQueue, view); }

} // namespace gfx
