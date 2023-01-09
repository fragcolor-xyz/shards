#include "renderer.hpp"
#include "egui_render_pass.hpp"
#include "gfx/params.hpp"
#include <gfx/enums.hpp>
#include <gfx/pipeline_step.hpp>
#include <gfx/renderer.hpp>
#include <gfx/shader/blocks.hpp>
#include <gfx/shader/types.hpp>
#include <gfx/mesh.hpp>
#include <gfx/texture.hpp>
#include <gfx/fwd.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/sized_item_pool.hpp>
#include <gfx/feature.hpp>
#include <gfx/view.hpp>
#include <gfx/context.hpp>
#include <gfx/window.hpp>
#include <gfx/gfx_wgpu.hpp>
#include <gfx/log.hpp>
#include <spdlog/spdlog.h>
#include <map>
#include <vector>
#include <memory>

namespace gfx {

static auto logger = getLogger();

struct MeshPoolOps {
  size_t getCapacity(MeshPtr &item) const { return item->getNumVertices() * item->getFormat().getVertexSize(); }
  void init(MeshPtr &item, size_t size) const { item = std::make_shared<Mesh>(); }
};
typedef detail::SizedItemPool<MeshPtr, MeshPoolOps> MeshPool;

struct TextureManager {
  std::map<uint64_t, TexturePtr> textures;

  TexturePtr get(const egui::TextureId &id) const {
    if (id.managed) {
      auto it = textures.find(id.id);
      if (it != textures.end())
        return it->second;
      return TexturePtr();
    } else {
      TexturePtr texture = *reinterpret_cast<TexturePtr *>(id.id);
      return texture;
    }
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

  void set(const egui::TextureSet &set) {
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
    case egui::TextureFormat::R32F:
      //  Need to convert, 32-bit float types are not filterable in the WebGPU spec
      srcFormat = WGPUTextureFormat_R32Float;
      fmt.pixelFormat = WGPUTextureFormat_R8Unorm;
      srcPixelSize = sizeof(float);
      dstPixelSize = sizeof(uint8_t);
      break;
    case egui::TextureFormat::RGBA8:
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
      uint8_t *dstBase = imageData.data() + set.offset[0] * dstPixelSize;
      for (size_t srcY = 0; srcY < set.size[1]; srcY++) {
        size_t dstY = set.offset[1] + srcY;
        uint8_t *dstPtr = dstBase + dstPitch * dstY;
        const uint8_t *srcPtr = set.pixels + srcPitch * srcY;
        imageCopy(fmt.pixelFormat, srcFormat, dstPtr, srcPtr, set.size[0]);
      }
    } else { // Full image
      size.x = set.size[0];
      size.y = set.size[1];
      imageCopy(fmt.pixelFormat, srcFormat, imageData.data(), set.pixels, size.x * size.y);
    }

    texture->init(TextureDesc{
        .format = fmt,
        .resolution = size,
        .samplerState = sampler,
        .data = std::move(imageData),
    });
  }

  void free(egui::TextureId id) { textures.erase(id.id); }
};

struct EguiRendererImpl {
  MeshPool meshPool;
  TextureManager textures;
  std::vector<egui::TextureId> pendingTextureFrees;
  std::vector<MeshDrawable> drawables;
  std::vector<FeaturePtr> uiFeatures;

  MeshFormat meshFormat;

  EguiRendererImpl() {
    uiFeatures.push_back(EguiColorFeature::create());
    meshFormat = {
        .primitiveType = PrimitiveType::TriangleList,
        .windingOrder = WindingOrder::CCW,
        .indexFormat = IndexFormat::UInt32,
        .vertexAttributes = egui::Vertex::getAttributes(),
    };
  }

  void addPendingTextureFree(egui::TextureId id) { pendingTextureFrees.push_back(id); }

  void processPendingTextureFrees() {
    for (auto &id : pendingTextureFrees) {
      SPDLOG_LOGGER_INFO(logger, "textureFree {}", (uint64_t)id.id);
      textures.free(id);
    }
    pendingTextureFrees.clear();
  }

  void render(const egui::FullOutput &output, const float4x4 &rootTransform, const gfx::DrawQueuePtr &drawQueue) {
    meshPool.reset();
    processPendingTextureFrees();

    // Update textures before render
    auto &textureUpdates = output.textureUpdates;
    for (size_t i = 0; i < textureUpdates.numSets; i++) {
      auto &textureSet = textureUpdates.sets[i];
      SPDLOG_LOGGER_INFO(logger, "textureSet {}", (uint64_t)textureSet.id);
      textures.set(textureSet);
    }

    // Update meshes & generate drawables
    drawables.resize(output.numPrimitives);
    for (size_t i = 0; i < output.numPrimitives; i++) {
      auto &prim = output.primitives[i];

      MeshPtr mesh = meshPool.allocateBuffer(prim.numVertices);
      mesh->update(meshFormat, prim.vertices, prim.numVertices * sizeof(egui::Vertex), prim.indices,
                   prim.numIndices * sizeof(uint32_t));

      MeshDrawable &drawable = drawables[i];
      drawable.mesh = mesh;
      drawable.transform = rootTransform;
      drawable.features = uiFeatures;
      TexturePtr texture = textures.get(prim.textureId);
      if (texture) {
        drawable.parameters.set("color", texture);
        if (texture->getFormat().pixelFormat == WGPUTextureFormat_R8Unorm) {
          drawable.parameters.set("flags", uint32_t(0x1));
        }
      }

      // drawable.clipRect = int4(prim.clipRect.min.x, prim.clipRect.min.y, prim.clipRect.max.x, prim.clipRect.max.y);

      drawQueue->add(drawable);
    }

    // Store texture id's to free
    // these are executed at the start the next time render() is called
    for (size_t i = 0; i < textureUpdates.numFrees; i++) {
      auto &id = textureUpdates.frees[i];
      addPendingTextureFree(id);
    }
  }
};

EguiRenderer::EguiRenderer() { impl = std::make_shared<EguiRendererImpl>(); }

void EguiRenderer::render(const egui::FullOutput &output, const float4x4 &rootTransform, const gfx::DrawQueuePtr &drawQueue) {
  impl->render(output, rootTransform, drawQueue);
}

void EguiRenderer::renderNoTransform(const egui::FullOutput &output, const gfx::DrawQueuePtr &drawQueue) {
  render(output, linalg::identity, drawQueue);
}

EguiRenderer *EguiRenderer::create() { return new EguiRenderer(); }

void EguiRenderer::destroy(EguiRenderer *renderer) { delete renderer; }

float EguiRenderer::getDrawScale(Window &window) {
  float2 drawScaleVec = window.getUIScale();
  return std::max<float>(drawScaleVec.x, drawScaleVec.y);
}
} // namespace gfx
