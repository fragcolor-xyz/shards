#include "egui_interop.hpp"
#include "egui_render_pass.hpp"
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

struct RendererImpl {
  MeshPool meshPool;
  TextureManager textures;
  std::vector<TextureId> pendingTextureFrees;

  MeshFormat meshFormat;

  RendererImpl() {
    meshFormat = {
        .primitiveType = PrimitiveType::TriangleList,
        .windingOrder = WindingOrder::CCW,
        .indexFormat = IndexFormat::UInt32,
        .vertexAttributes = Vertex::getAttributes(),
    };
  }

  void addPendingTextureFree(TextureId id) { pendingTextureFrees.push_back(id); }

  void processPendingTextureFrees() {
    for (auto &id : pendingTextureFrees) {
      SPDLOG_INFO("textureFree {}", (uint64_t)id.id);
      textures.free(id);
    }
    pendingTextureFrees.clear();
  }
};

Renderer::Renderer() { impl = std::make_shared<RendererImpl>(); }
void Renderer::render(const FullOutput &output, const gfx::DrawQueuePtr &drawQueue) {
  impl->meshPool.reset();
  impl->processPendingTextureFrees();

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
    drawQueue->add(drawable);
  }

  // Store texture id's to free
  // these are executed at the start the next time render() is called
  for (size_t i = 0; i < textureUpdates.numFrees; i++) {
    auto &id = textureUpdates.frees[i];
    impl->addPendingTextureFree(id);
  }
}

Renderer *Renderer::create() { return new Renderer(); }
void Renderer::destroy(Renderer *renderer) { delete renderer; }
} // namespace egui

namespace gfx {
Context &Renderer_getContext(Renderer &renderer) { return renderer.getContext(); }
egui::Rect Context_getScreenRect(gfx::Context &context) {
  Window &window = context.getWindow();
  float2 drawScale = window.getDrawScale();
  float2 screenSize = window.getDrawableSize() / std::max(drawScale.x, drawScale.y);
  return egui::Rect{
      .min = egui::Pos2{0.0f, 0.0f},
      .max = egui::Pos2{screenSize.x, screenSize.y},
  };
}
float Context_getDrawScale(gfx::Context &context) {
  Window &window = context.getWindow();
  float2 drawScale = window.getDrawScale();
  return std::max(drawScale.x, drawScale.y);
}
} // namespace gfx
