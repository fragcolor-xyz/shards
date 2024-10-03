#ifndef E332B211_27DC_4F7F_BFCD_CF63EE8257FB
#define E332B211_27DC_4F7F_BFCD_CF63EE8257FB

#include "texture.hpp"
#include <shards/core/serialization/generic.hpp>
#include <shards/core/serialization/linalg.hpp>

namespace gfx {
struct TextureAssetHeader {
  std::string name;
  ::gfx::TextureFormat format;
};
} // namespace gfx

namespace shards {

template <> struct Serialize<gfx::TextureAssetHeader> {
  template <SerializerStream S> void operator()(S &stream, gfx::TextureAssetHeader &v) {
    serde(stream, v.name);
    serde(stream, v.format);
  }
};

template <> struct Serialize<::gfx::TextureFormat> {
  template <SerializerStream S> void operator()(S &stream, ::gfx::TextureFormat &v) {
    serde(stream, v.resolution);
    serdeAs<uint8_t>(stream, v.dimension);
    serdeAs<uint8_t>(stream, v.flags);
    serdeAs<uint32_t>(stream, v.pixelFormat);
    serdeAs<uint8_t>(stream, v.mipLevels);
  }
};
} // namespace shards

#endif /* E332B211_27DC_4F7F_BFCD_CF63EE8257FB */
