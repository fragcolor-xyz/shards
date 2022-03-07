#include "renderer.hpp"
#include "context.hpp"
#include "drawable.hpp"
#include "fields.hpp"
#include "hasherxxh128.hpp"
#include "mesh.hpp"
#include "view.hpp"
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

namespace gfx {

struct UniformLayout {
  size_t offset;
  size_t size;
  FieldType type;
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

  UniformLayout generateNext(FieldType fieldType) const {
    UniformLayout result;
    result.offset = offset;
    result.size = getFieldTypeSize(fieldType);
    result.type = fieldType;
    return result;
  }

  const UniformLayout &push(const std::string &name, UniformLayout &&layout) {
    const UniformLayout &result = bufferLayout.push(name, std::move(layout));
    offset = std::max(offset, layout.offset + layout.size);
    return result;
  }

  const UniformLayout &push(const std::string &name, FieldType fieldType) { return push(name, generateNext(fieldType)); }

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
  WGPUPipelineLayout pipelineLayout = {};
  std::vector<WGPUBindGroupLayout> bindGroupLayouts;
  void release() {
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(shaderModule);
    wgpuRenderPipelineRelease(pipeline);
    for (WGPUBindGroupLayout &layout : bindGroupLayouts) {
      wgpuBindGroupLayoutRelease(layout);
    }
  }
};
typedef std::shared_ptr<PipelineInternal> PipelineInternalPtr;

struct PipelineGroup {
  Mesh::Format meshFormat;
  // BindingLayout bindingLayout;
  WGPUBuffer objectBuffer;
  WGPUBindGroup bindGroup;
  std::vector<DrawablePtr> drawables;
};

static void packDrawData(uint8_t *outData, size_t outSize, const UniformBufferLayout &layout, const DrawData &drawData) {
  for (auto &pair : layout.mapping) {
    const std::string &key = pair.first;
    auto drawDataIt = drawData.data.find(key);
    if (drawDataIt != drawData.data.end()) {
      const UniformLayout &itemLayout = layout.items[pair.second];
      FieldType drawDataFieldType = getFieldVariantType(drawDataIt->second);
      if (itemLayout.type != drawDataFieldType) {
        spdlog::warn("WEBGPU: Field type mismatch layout:{} drawData:{}", magic_enum::enum_name(itemLayout.type),
                     magic_enum::enum_name(drawDataFieldType));
        continue;
      }

      packFieldVariant(outData + itemLayout.offset, outSize - itemLayout.offset, drawDataIt->second);
    }
  }
}

struct RendererImpl {
  Context &context;
  WGPUSupportedLimits deviceLimits = {};
  WGPUSupportedLimits adapterLimits = {};

  UniformBufferLayout viewBufferLayout;
  UniformBufferLayout objectBufferLayout;
  std::vector<std::function<void()>> postFrameQueue;

  RendererImpl(Context &context) : context(context) {
    UniformBufferLayoutBuilder viewBufferLayoutBuilder;
    viewBufferLayoutBuilder.push("view", FieldType::Float4x4);
    viewBufferLayoutBuilder.push("proj", FieldType::Float4x4);
    viewBufferLayoutBuilder.push("invView", FieldType::Float4x4);
    viewBufferLayoutBuilder.push("invProj", FieldType::Float4x4);
    viewBufferLayoutBuilder.push("viewSize", FieldType::Float4x4);
    viewBufferLayout = viewBufferLayoutBuilder.finalize();

    UniformBufferLayoutBuilder objectBufferLayoutBuilder;
    objectBufferLayoutBuilder.push("world", FieldType::Float4x4);
    objectBufferLayout = objectBufferLayoutBuilder.finalize();

    wgpuDeviceGetLimits(context.wgpuDevice, &deviceLimits);
    wgpuAdapterGetLimits(context.wgpuAdapter, &adapterLimits);
  }

  ~RendererImpl() { processPostFrameQueue(); }

  size_t alignToMinUniformOffset(size_t size) const {
    size_t alignment = deviceLimits.limits.minUniformBufferOffsetAlignment;
    if (alignment == 0)
      return size;

    size_t remainder = size % alignment;
    if (remainder > 0) {
      return size + (alignment - remainder);
    }
    return size;
  }

  void renderView(const DrawQueue &drawQueue, ViewPtr view) {
    Rect viewport;
    int2 viewSize;
    if (view->viewport) {
      viewSize = view->viewport->getSize();
      viewport = view->viewport.value();
    } else {
      viewSize = context.getMainOutputSize();
      viewport = Rect(0, 0, viewSize.x, viewSize.y);
    }

    DrawData viewDrawData;
    viewDrawData.set_float4x4("view", view->view);
    viewDrawData.set_float4x4("invView", linalg::inverse(view->view));
    float4x4 projMatrix = view->getProjectionMatrix(viewSize);
    viewDrawData.set_float4x4("proj", projMatrix);
    viewDrawData.set_float4x4("invProj", linalg::inverse(projMatrix));

    WGPUBuffer viewBuffer = createTransientUniformBuffer(viewBufferLayout.size, "viewUniform");
    std::vector<uint8_t> stagingBuffer;
    stagingBuffer.resize(viewBufferLayout.size);
    packDrawData(stagingBuffer.data(), stagingBuffer.size(), viewBufferLayout, viewDrawData);
    wgpuQueueWriteBuffer(context.wgpuQueue, viewBuffer, 0, stagingBuffer.data(), stagingBuffer.size());

    renderDrawablePass(drawQueue, view, viewport, viewBuffer);
  }

  WGPUBuffer createTransientUniformBuffer(size_t size, const char *label = nullptr) {
    WGPUBufferDescriptor desc = {};
    desc.size = size;
    desc.label = label;
    desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(context.wgpuDevice, &desc);
    onPostFrame([=]() { wgpuBufferRelease(buffer); });
    return buffer;
  }

  void onPostFrame(std::function<void()> &&callback) { postFrameQueue.emplace_back(std::move(callback)); }
  void processPostFrameQueue() {
    for (auto &cb : postFrameQueue) {
      cb();
    }
    postFrameQueue.clear();
  }

  struct Bindable {
    WGPUBuffer buffer;
    UniformBufferLayout layout;
    Bindable(WGPUBuffer buffer, UniformBufferLayout layout) : buffer(buffer), layout(layout) {}
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
      entry.size = bindable.layout.size;
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
    mainAttach.view = wgpuSwapChainGetCurrentTextureView(context.wgpuSwapchain);
    mainAttach.storeOp = WGPUStoreOp_Store;
    passDesc.colorAttachments = &mainAttach;
    passDesc.colorAttachmentCount = 1;

    std::unordered_map<Hash128, PipelineGroup> groups;
    groupByPipeline(drawQueue.getDrawables(), groups);
    for (auto &pair : groups) {
      PipelineGroup &group = pair.second;

      size_t alignedObjectBufferSize = alignToMinUniformOffset(objectBufferLayout.size);
      size_t numObjects = group.drawables.size();
      size_t bufferLength = numObjects * alignedObjectBufferSize;
      group.objectBuffer = createTransientUniformBuffer(bufferLength, "objects");

      std::vector<uint8_t> drawDataTempBuffer;
      drawDataTempBuffer.resize(bufferLength);
      for (size_t i = 0; i < numObjects; i++) {
        DrawablePtr drawable = group.drawables[i];
        drawable->mesh->createContextDataCondtional(&context);

        DrawData worldDrawData;
        worldDrawData.set_float4x4("world", drawable->transform);

        size_t bufferOffset = alignedObjectBufferSize * i;
        size_t remainingBufferLength = bufferLength - bufferOffset;
        packDrawData(drawDataTempBuffer.data() + bufferOffset, remainingBufferLength, objectBufferLayout, worldDrawData);
      }

      wgpuQueueWriteBuffer(context.wgpuQueue, group.objectBuffer, 0, drawDataTempBuffer.data(), drawDataTempBuffer.size());
    }

    WGPURenderPassEncoder passEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);
    wgpuRenderPassEncoderSetViewport(passEncoder, (float)viewport.x, (float)viewport.y, (float)viewport.width,
                                     (float)viewport.height, 0.0f, 1.0f);

    for (auto &pair : groups) {
      PipelineGroup &group = pair.second;
      PipelineInternalPtr pipeline = buildPipeline(group);
      onPostFrame([=]() { pipeline->release(); });

      std::vector<Bindable> bindables = {
          Bindable(viewBuffer, viewBufferLayout),
          Bindable(group.objectBuffer, objectBufferLayout),
      };
      WGPUBindGroup bindGroup = createBindGroup(device, pipeline->bindGroupLayouts[0], bindables);
      onPostFrame([=]() { wgpuBindGroupRelease(bindGroup); });

      for (size_t i = 0; i < group.drawables.size(); i++) {
        auto &drawable = group.drawables[i];
        auto &mesh = drawable->mesh;

        size_t alignedObjectBufferSize = alignToMinUniformOffset(objectBufferLayout.size);
        uint32_t objectBufferOffset = i * alignedObjectBufferSize;
        wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, bindGroup, 1, &objectBufferOffset);
        wgpuRenderPassEncoderSetPipeline(passEncoder, pipeline->pipeline);
        wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, mesh->contextData.vertexBuffer, 0, 0);
        if (mesh->contextData.indexBuffer) {
          wgpuRenderPassEncoderSetIndexBuffer(passEncoder, mesh->contextData.indexBuffer,
                                              getWGPUIndexFormat(mesh->getFormat().indexFormat), 0, 0);
          wgpuRenderPassEncoderDrawIndexed(passEncoder, (uint32_t)mesh->getNumIndices(), 1, 0, 0, 0);
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
      }

      PipelineGroup &group = it->second;
      group.drawables.push_back(drawable);
    }
  }

  std::shared_ptr<PipelineInternal> buildPipeline(PipelineGroup &group) {
    WGPUDevice device = context.wgpuDevice;

    std::shared_ptr<PipelineInternal> result = std::make_shared<PipelineInternal>();

    WGPUShaderModuleDescriptor moduleDesc = {};
    WGPUShaderModuleWGSLDescriptor wgslModuleDesc = {};
    wgslModuleDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    moduleDesc.label = "pipeline";
    wgslModuleDesc.code = R"(
			struct View {
				view: mat4x4<f32>;
				proj: mat4x4<f32>;
				invView: mat4x4<f32>;
				invProj: mat4x4<f32>;
				viewSize: mat4x4<f32>;
			};

			struct Object {
				world: mat4x4<u32>;
			};

			[[group(0), binding(0)]]
			var<uniform> u_view: View;

			[[group(0), binding(1)]]
			var<uniform> u_object: Object;

			struct VertToFrag {
				[[builtin(position)]] position: vec4<f32>;
				[[location(0)]] normal: vec3<f32>;
			};

			[[stage(vertex)]]
			fn vs_main([[location(0)]] position: vec3<f32>, [[location(1)]] normal: vec3<f32>) -> VertToFrag {
				var v2f: VertToFrag;
				let worldPosition = u_object.world * vec4<f32>(position.x, position.y, position.z, 1.0);
				v2f.position = u_view.proj * u_view.view * worldPosition;
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
      WGPUBindGroupLayoutEntry &objectEntry = bindGroupLayoutEntries.emplace_back();
      objectEntry.binding = 1;
      objectEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
      objectEntry.buffer.type = WGPUBufferBindingType_Uniform;
      objectEntry.buffer.hasDynamicOffset = true;
      objectEntry.buffer.minBindingSize = objectBufferLayout.size;
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

    desc.primitive.cullMode = WGPUCullMode_None; // TODO
    desc.primitive.frontFace = group.meshFormat.windingOrder == WindingOrder::CCW ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
    switch (group.meshFormat.primitiveType) {
    case PrimitiveType::TriangleList:
      desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
      break;
    case PrimitiveType::TriangleStrip:
      desc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
      desc.primitive.stripIndexFormat = getWGPUIndexFormat(group.meshFormat.indexFormat);
      break;
    }

    result->pipeline = wgpuDeviceCreateRenderPipeline(device, &desc);
    errorScope.pop(
        [](WGPUErrorType type, char const *message) { spdlog::error("Failed to create pipeline({}): {}", type, message); });
    return result;
  };
};

Renderer::Renderer(Context &context) { impl = std::make_shared<RendererImpl>(context); }

void Renderer::postFrameCleanup() { impl->processPostFrameQueue(); }
void Renderer::render(const DrawQueue &drawQueue, ViewPtr view) { impl->renderView(drawQueue, view); }

} // namespace gfx
