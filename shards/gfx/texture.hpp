#ifndef GFX_TEXTURE
#define GFX_TEXTURE

#include "context_data.hpp"
#include "wgpu_handle.hpp"
#include "gfx_wgpu.hpp"
#include "isb.hpp"
#include "linalg.hpp"
#include "unique_id.hpp"
#include "fwd.hpp"
#include "enums.hpp"
#include "log.hpp"
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
  std::weak_ptr<Texture> keepAliveRef;

  TextureFormat format{};
  // Only set for managed textures
  WgpuHandle<WGPUTexture> texture{};
  // Only set for externally managed textures, not freed
  WGPUTexture externalTexture{};
  WGPUExtent3D size{};
  UniqueId id{};

#if SH_GFX_CONTEXT_DATA_LABELS
  std::string label;
  std::string_view getLabel() const { return label; }
#endif

  TextureContextData() = default;
  TextureContextData(TextureContextData &&) = default;

  void init(std::string_view label) {
#if SH_GFX_CONTEXT_DATA_LABELS
    this->label = label;
#endif
#if SH_GFX_CONTEXT_DATA_LOG_LIFETIME
    SPDLOG_LOGGER_DEBUG(getContextDataLogger(), "Texture {} data created", getLabel());
#endif
  }

#if SH_GFX_CONTEXT_DATA_LOG_LIFETIME
  ~TextureContextData() {
    if (texture)
      SPDLOG_LOGGER_DEBUG(getContextDataLogger(), "Texture {} data destroyed", getLabel());
  }
#endif
};

struct TextureSource {
  uint8_t numChannels = 4;
  uint32_t rowStride{};
  ImmutableSharedBuffer data;

  std::strong_ordering operator<=>(const TextureSource &other) const = default;
};

/// <div rustbindgen opaque></div>
struct TextureDesc {
  TextureFormat format;
  int2 resolution;
  TextureSource source;

  // Can wrap an already existing texture if this is passed
  // it will not be released when the texture object is destroyed
  std::optional<WGPUTexture> externalTexture;

  static TextureDesc getDefault();

  std::strong_ordering operator<=>(const TextureDesc &other) const = default;
};

/// <div rustbindgen opaque></div>
struct Texture final {
  using ContextDataType = TextureContextData;

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
  TextureSource getSource() const { return desc.source; }
  const TextureFormat &getFormat() const { return desc.format; }
  int2 getResolution() const { return desc.resolution; }

  TexturePtr clone() const;

  UniqueId getId() const { return id; }
  size_t getVersion() const { return version; }

  bool keepAlive() const { return true; }

  /// <div rustbindgen hide></div>
  static TexturePtr makeRenderAttachment(WGPUTextureFormat format, std::string &&label);

  void initContextData(Context &context, TextureContextData &contextData);
  void updateContextData(Context &context, TextureContextData &contextData);

protected:
  static UniqueId getNextId();

private:
  void update();
};

static_assert(TWithContextDataKeepAlive<Texture>, "");

} // namespace gfx

#endif // GFX_TEXTURE
