#ifndef GFX_TEXTURE
#define GFX_TEXTURE

#include "context_data.hpp"
#include "gfx_wgpu.hpp"
#include "isb.hpp"
#include "linalg.hpp"
#include "unique_id.hpp"
#include "fwd.hpp"
#include "enums.hpp"
#include <compare>
#include <variant>
#include <optional>
#include <string>
#include <boost/container_hash/hash_fwd.hpp>

namespace gfx {
struct SamplerState {
  WGPUAddressMode addressModeU = WGPUAddressMode::WGPUAddressMode_Repeat;
  WGPUAddressMode addressModeV = WGPUAddressMode::WGPUAddressMode_Repeat;
  WGPUAddressMode addressModeW = WGPUAddressMode::WGPUAddressMode_Repeat;
  WGPUFilterMode filterMode = WGPUFilterMode::WGPUFilterMode_Linear;

  template <typename T> void getPipelineHash(T &hasher) const {
    hasher(addressModeU);
    hasher(addressModeV);
    hasher(addressModeW);
    hasher(filterMode);
  }

  std::strong_ordering operator<=>(const SamplerState &) const = default;

  friend size_t hash_value(SamplerState const &v) {
    size_t result{};
    boost::hash_combine(result, uint32_t(v.addressModeU));
    boost::hash_combine(result, uint32_t(v.addressModeV));
    boost::hash_combine(result, uint32_t(v.addressModeW));
    boost::hash_combine(result, uint32_t(v.filterMode));
    return result;
  }
};

struct TextureFormat {
  TextureDimension dimension = TextureDimension::D2;
  TextureFormatFlags flags = TextureFormatFlags::AutoGenerateMips;
  WGPUTextureFormat pixelFormat = WGPUTextureFormat::WGPUTextureFormat_Undefined;
  uint8_t mipLevels = 1;

  bool hasMips() { return mipLevels > 1; }
  std::strong_ordering operator<=>(const TextureFormat &other) const = default;
};

struct InputTextureFormat {
  uint8_t pixelSize;
  size_t numComponents;
};

/// <div rustbindgen opaque></div>
struct TextureContextData : public ContextData {
  TextureFormat format;
  // Only set for managed textures
  WGPUTexture texture{};
  // Only set for externally managed textures, not freed
  WGPUTexture externalTexture{};
  WGPUExtent3D size{};
  UniqueId id;
  size_t version;

  ~TextureContextData() { releaseContextDataConditional(); }

  void releaseContextData() override { WGPU_SAFE_RELEASE(wgpuTextureRelease, texture); }
};

/// <div rustbindgen opaque></div>
struct TextureDesc {
  TextureFormat format;
  int2 resolution;
  ImmutableSharedBuffer data;

  // Can wrap an already existing texture if this is passed
  // it will not be released when the texture object is destroyed
  std::optional<WGPUTexture> externalTexture;

  static TextureDesc getDefault();

  std::strong_ordering operator<=>(const TextureDesc &other) const = default;
};

/// <div rustbindgen opaque></div>
struct Texture final : public TWithContextData<TextureContextData> {
private:
  UniqueId id = getNextId();
  TextureDesc desc = TextureDesc::getDefault();
  SamplerState samplerState{};
  std::string label;
  size_t version{};

  friend struct gfx::UpdateUniqueId<Texture>;

public:
  Texture() = default;
  Texture(std::string &&label) : label(label) {}

  // Creates a texture
  Texture &init(const TextureDesc &desc);
  Texture &initWithSamplerState(const SamplerState &samplerState);
  Texture &initWithResolution(int2 resolution);
  Texture &initWithFlags(TextureFormatFlags formatFlags);
  Texture &initWithPixelFormat(WGPUTextureFormat format);
  /// <div rustbindgen hide></div>
  Texture &initWithLabel(std::string &&label);

  SamplerState &getSamplerState() { return samplerState; }
  const std::string &getLabel() const { return label; }
  const TextureDesc &getDesc() const { return desc; }
  ImmutableSharedBuffer getData() const { return desc.data; }
  const TextureFormat &getFormat() const { return desc.format; }
  int2 getResolution() const { return desc.resolution; }

  TexturePtr clone() const;

  UniqueId getId() const { return id; }
  size_t getVersion() const { return version; }

  /// <div rustbindgen hide></div>
  static TexturePtr makeRenderAttachment(WGPUTextureFormat format, std::string &&label);

protected:
  void initContextData(Context &context, TextureContextData &contextData);
  void updateContextData(Context &context, TextureContextData &contextData);

  static UniqueId getNextId();

private:
  void update();
};

} // namespace gfx

#endif // GFX_TEXTURE
