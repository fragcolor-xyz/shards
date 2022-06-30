#include "egui_interop.hpp"
#include "gfx/params.hpp"
#include <gfx/enums.hpp>
#include <gfx/pipeline_step.hpp>
#include <gfx/renderer.hpp>
#include <gfx/shader/blocks.hpp>
#include <gfx/shader/types.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <gfx/mesh.hpp>
#include <gfx/texture.hpp>
#include <gfx/fwd.hpp>
#include <gfx/drawable.hpp>
#include <gfx/resizable_item_pool.hpp>
#include <gfx/feature.hpp>
#include <gfx/view.hpp>
#include <gfx/context.hpp>
#include <gfx/window.hpp>
#include <map>
#include <vector>
#include <webgpu-headers/webgpu.h>
using namespace gfx;

namespace egui {

std::vector<MeshVertexAttribute> Vertex::getAttributes() {
  return std::vector<MeshVertexAttribute>{
      MeshVertexAttribute("position", 2),
      MeshVertexAttribute("texCoord0", 2),
      MeshVertexAttribute("color", 4, VertexAttributeType::UNorm8),
  };
}

struct MeshPoolOps {
  size_t getCapacity(MeshPtr &item) const { return item->getNumVertices() * item->getFormat().getVertexSize(); }
  void init(MeshPtr &item) const { item = std::make_shared<Mesh>(); }
};
typedef ResizableItemPool<MeshPtr, MeshPoolOps> MeshPool;

struct TextureManager {
  std::map<uint64_t, TexturePtr> textures;

  TexturePtr get(const TextureId &id) const {
    auto it = textures.find(id.id);
    if (it != textures.end())
      return it->second;
    return TexturePtr();
  }

  void imageCopy(WGPUTextureFormat dstFormat, WGPUTextureFormat srcFormat, uint8_t *dst, const uint8_t *src, size_t numPixels) {
    switch (srcFormat) {
    case WGPUTextureFormat_RGBA8UnormSrgb:
      assert(dstFormat == srcFormat);
      memcpy(dst, src, numPixels * sizeof(uint32_t));
      break;
    case WGPUTextureFormat_R32Float: {
      assert(dstFormat == WGPUTextureFormat_R8Unorm);
      const float *srcFloat = (float *)src;
      for (size_t i = 0; i < numPixels; i++) {
        *dst = uint8_t(*srcFloat * 255);
        dst++;
        srcFloat++;
      }
    } break;
    default:
      assert(false);
    }
  }

  void set(const TextureSet &set) {
    auto it = textures.find(set.id);
    if (it == textures.end()) {
      it = textures.insert_or_assign(set.id.id, std::make_shared<Texture>()).first;
    }
    TexturePtr texture = it->second;

    gfx::TextureFormat fmt{};
    WGPUTextureFormat srcFormat;
    size_t srcPixelSize;
    size_t dstPixelSize;
    switch (set.format) {
    case TextureFormat::R32F:
      //  Need to convert, 32-bit float types are not filterable in the WebGPU spec
      srcFormat = WGPUTextureFormat_R32Float;
      fmt.pixelFormat = WGPUTextureFormat_R8Unorm;
      srcPixelSize = sizeof(float);
      dstPixelSize = sizeof(uint8_t);
      break;
    case TextureFormat::RGBA8:
      srcFormat = fmt.pixelFormat = WGPUTextureFormat_RGBA8UnormSrgb;
      srcPixelSize = dstPixelSize = sizeof(uint32_t);
      break;
    }

    fmt.type = TextureType::D2;
    fmt.flags = TextureFormatFlags::None;
    SamplerState sampler{
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_ClampToEdge,
        .filterMode = WGPUFilterMode_Linear,
    };

    ImmutableSharedBuffer currentData = texture->getData();
    std::vector<uint8_t> imageData;
    if (set.subRegion && currentData.getLength() > 0) {
      imageData.resize(currentData.getLength());
      memcpy(imageData.data(), currentData.getData(), currentData.getLength());
    } else {
      if (set.subRegion)
        throw std::logic_error("Can not update a sub-region on a non-existing texture");
      imageData.resize(dstPixelSize * set.size[0] * set.size[1]);
    }

    int2 size{};
    if (set.subRegion) {
      size = texture->getResolution();
      size_t dstPitch = size.x * dstPixelSize;
      size_t srcPitch = set.size[0] * srcPixelSize;
      for (size_t y = set.offset[0]; y < set.size[1]; y++) {
        uint8_t *dstPtr = imageData.data() + dstPitch * y;
        const uint8_t *srcPtr = set.pixels + srcPitch * y;
        imageCopy(fmt.pixelFormat, srcFormat, dstPtr, srcPtr, set.size[0]);
      }
    } else { // Full image
      size.x = set.size[0];
      size.y = set.size[1];
      imageCopy(fmt.pixelFormat, srcFormat, imageData.data(), set.pixels, size.x * size.y);
    }

    texture->init(fmt, int2(set.size[0], set.size[1]), sampler, std::move(imageData));
  }

  void free(TextureId id) { textures.erase(id.id); }
};

struct EguiRendererImpl {
  Renderer &renderer;
  MeshPool meshPool;
  TextureManager textures;

  MeshFormat meshFormat;
  std::shared_ptr<DrawQueue> queue;
  ViewPtr view;
  FeaturePtr uiFeature;
  PipelineSteps pipelineSteps;

  EguiRendererImpl(Renderer &renderer) : renderer(renderer) {
    uiFeature = createUIFeature();
    queue = std::make_shared<DrawQueue>();
    view = std::make_shared<View>();

    meshFormat = {
        .primitiveType = PrimitiveType::TriangleList,
        .windingOrder = WindingOrder::CCW,
        .indexFormat = IndexFormat::UInt32,
        .vertexAttributes = Vertex::getAttributes(),
    };

    auto drawableStep = makeDrawablePipelineStep(RenderDrawablesStep{
        .drawQueue = queue,
        .features = std::vector<FeaturePtr>{uiFeature},
        .sortMode = SortMode::Queue,
    });
    pipelineSteps = PipelineSteps{drawableStep};
  }

  static FeaturePtr createUIFeature() {
    auto uiFeature = std::make_shared<Feature>();
    auto code = shader::blocks::makeCompoundBlock();

    code->append(shader::blocks::Header(R"(
fn linear_from_srgb(srgb: vec3<f32>) -> vec3<f32> {
  let srgb_bytes = srgb * 255.0;
  let cutoff = srgb_bytes < vec3<f32>(10.31475);
  let lower = srgb_bytes / vec3<f32>(3294.6);
  let higher = pow((srgb_bytes + vec3<f32>(14.025)) / vec3<f32>(269.025), vec3<f32>(2.4));
  return select(higher, lower, cutoff);
})"));
    code->appendLine("var vp = ", shader::blocks::ReadBuffer("viewport", shader::FieldTypes::Float4, "view"));
    code->appendLine("var p1 = ", shader::blocks::ReadInput("position"), " * (1.0 / vp.zw)");
    code->appendLine("p1.y = -p1.y;");
    code->appendLine("p1 = p1 * 2.0 + vec2<f32>(-1.0, 1.0)");
    code->appendLine("var p4 = vec4<f32>(p1.xy, 0.0, 1.0)");
    code->appendLine(shader::blocks::WriteOutput("position", shader::FieldTypes::Float4, "p4"));
    code->appendLine("var color = ", shader::blocks::ReadInput("color"));
    code->appendLine(
        shader::blocks::WriteOutput("color", shader::FieldTypes::Float4, "vec4<f32>(linear_from_srgb(color.xyz), color.a)"));

    uiFeature->shaderEntryPoints.emplace_back("baseTransform", ProgrammableGraphicsStage::Vertex, std::move(code));

    shader::FieldType flagsFieldType = shader::FieldType(ShaderFieldBaseType::UInt32, 1);
    code = shader::blocks::makeCompoundBlock();
    code->appendLine("var color = ", shader::blocks::ReadInput("color"));
    code->appendLine("var texColor = ", shader::blocks::SampleTexture("color"));
    code->appendLine("var isFont = ", shader::blocks::ReadBuffer("flags", flagsFieldType), " & 1u");
    code->append("if(isFont != 0u) { texColor = vec4<f32>(texColor.xxx, texColor.x); }\n");
    code->appendLine(shader::blocks::WriteOutput("color", shader::FieldTypes::Float4, "color * texColor"));
    uiFeature->shaderEntryPoints.emplace_back("color", ProgrammableGraphicsStage::Fragment, std::move(code));

    uiFeature->textureParams.emplace_back("color");
    uiFeature->shaderParams.emplace_back("flags", flagsFieldType, uint8_t(0));

    uiFeature->state.set_depthWrite(false);
    uiFeature->state.set_depthCompare(WGPUCompareFunction_Always);
    uiFeature->state.set_culling(false);
    uiFeature->state.set_blend(BlendState{
        .color = BlendComponent::AlphaPremultiplied,
        .alpha = BlendComponent::Opaque,
    });
    return uiFeature;
  }
};

EguiRenderer::EguiRenderer(gfx::Renderer &renderer) { impl = std::make_shared<EguiRendererImpl>(renderer); }
void EguiRenderer::render(const FullOutput &output) {
  impl->meshPool.reset();
  impl->queue->clear();

  // Update textures before render
  auto &textureUpdates = output.textureUpdates;
  for (size_t i = 0; i < textureUpdates.numSets; i++) {
    auto &textureSet = textureUpdates.sets[i];
    SPDLOG_INFO("textureSet {}", (uint64_t)textureSet.id);
    impl->textures.set(textureSet);
  }

  // Update meshes & generate drawables
  for (size_t i = 0; i < output.numPrimitives; i++) {
    auto &prim = output.primitives[i];

    MeshPtr mesh = impl->meshPool.allocateBuffer(prim.numVertices);
    mesh->update(impl->meshFormat, prim.vertices, prim.numVertices * sizeof(Vertex), prim.indices,
                 prim.numIndices * sizeof(uint32_t));

    DrawablePtr drawable = std::make_shared<Drawable>(mesh);
    TexturePtr texture = impl->textures.get(prim.textureId);
    if (texture) {
      drawable->parameters.set("color", texture);
      if (texture->getFormat().pixelFormat == WGPUTextureFormat_R8Unorm) {
        drawable->parameters.set("flags", uint32_t(0x1));
      }
    }
    impl->queue->add(drawable);
  }

  impl->renderer.render(impl->view, impl->pipelineSteps);

  // Free textures after render
  for (size_t i = 0; i < textureUpdates.numFrees; i++) {
    auto &id = textureUpdates.frees[i];
    SPDLOG_INFO("textureFree {}", (uint64_t)id.id);
    impl->textures.free(id);
  }
}

void EguiRenderer::getScreenRect(Rect& outScreenRect) {
  Context &context = impl->renderer.getContext();
  Window &window = context.getWindow();
  float2 drawScale = window.getDrawScale();
  float2 screenSize = window.getDrawableSize() / std::max(drawScale.x, drawScale.y);
  outScreenRect = Rect{
      .min = Pos2{0.0f, 0.0f},
      .max = Pos2{screenSize.x, screenSize.y},
  };
}

float EguiRenderer::getScale() {
  Context &context = impl->renderer.getContext();
  Window &window = context.getWindow();
  float2 drawScale = window.getDrawScale();
  return std::max(drawScale.x, drawScale.y);
}
} // namespace egui
