#ifndef GFX_ENUMS
#define GFX_ENUMS

#include "gfx_wgpu.hpp"
#include <string_view>

namespace gfx {

enum class PrimitiveType { TriangleList, TriangleStrip };
enum class WindingOrder { CW, CCW };
enum class ProgrammableGraphicsStage { Vertex, Fragment };
enum class StorageType { Invalid, UInt8, Int8, UNorm8, SNorm8, UInt16, Int16, UNorm16, SNorm16, UInt32, Int32, Float16, Float32 };
enum class IndexFormat { UInt16, UInt32 };
enum class TextureSampleType {
  Int = 1 << 0,
  UInt = 1 << 1,
  Float = 1 << 2,
  UnfilterableFloat = 1 << 3,
  Depth = 1 << 4,
};

enum class TextureFormatFlags : uint8_t {
  None = 0x0,
  // Automatically generate mip-maps for this texture
  AutoGenerateMips = 0x01,
  // Allow this texture to be rendered to
  RenderAttachment = 0x02,
  // Indicate that this texture can not be sampled
  // Used to mark render attachments that can't be read from
  NoTextureBinding = 0x04,
  // Hint that this texture or derived objects shouldn't be cached between frames
  DontCache = 0x08,
};
inline TextureFormatFlags operator|(const TextureFormatFlags &a, const TextureFormatFlags &b) {
  return TextureFormatFlags(uint8_t(a) | uint8_t(b));
}
inline bool textureFormatFlagsContains(TextureFormatFlags left, TextureFormatFlags right) {
  return ((uint8_t &)left & (uint8_t &)right) != 0;
}

enum class TextureFormatUsage : uint8_t {
  Invalid = 0x0,
  Color = 0x1,
  Depth = 0x2,
  Stencil = 0x4,
};
inline TextureFormatUsage operator|(const TextureFormatUsage &a, const TextureFormatUsage &b) {
  return TextureFormatUsage(uint8_t(a) | uint8_t(b));
}
inline bool hasAnyTextureFormatUsage(const TextureFormatUsage &a, const TextureFormatUsage &b) {
  return (uint8_t(a) & uint8_t(b)) != 0;
}

enum class TextureDimension : uint8_t {
  D1,
  D2,
  Cube,
};

struct TextureFormatDesc {
  // What format components are stored in
  StorageType storageType;

  // Number of components
  size_t numComponents;

  // Number of bytes per pixel
  uint8_t pixelSize;

  TextureSampleType compatibleSampleTypes;

  TextureFormatUsage usage;

  TextureFormatDesc(StorageType storageType, size_t numComponents, TextureSampleType compatibleSampleTypes,
                    TextureFormatUsage usage = TextureFormatUsage::Color);
};

// TextureFormat
const TextureFormatDesc &getTextureFormatDescription(WGPUTextureFormat pixelFormat);
size_t getStorageTypeSize(const StorageType &type);
bool isIntegerStorageType(const StorageType &type);
size_t getIndexFormatSize(const IndexFormat &type);
WGPUVertexFormat getWGPUVertexFormat(const StorageType &type, size_t dim);
WGPUIndexFormat getWGPUIndexFormat(const IndexFormat &type);
WGPUTextureSampleType getWGPUSampleType(TextureSampleType type);

} // namespace gfx

#endif // GFX_ENUMS
