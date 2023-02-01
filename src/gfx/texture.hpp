#ifndef GFX_TEXTURE
#define GFX_TEXTURE

#include "context_data.hpp"
#include "gfx_wgpu.hpp"
#include "isb.hpp"
#include "linalg.hpp"
#include "unique_id.hpp"
#include "fwd.hpp"
#include "enums.hpp"
#include <variant>
#include <optional>
#include <string>

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
};

struct TextureFormat {
  TextureType type = TextureType::D2;
  TextureFormatFlags flags = TextureFormatFlags::AutoGenerateMips;
  WGPUTextureFormat pixelFormat = WGPUTextureFormat::WGPUTextureFormat_Undefined;

  bool hasMips() { return textureFormatFlagsContains(flags, TextureFormatFlags::AutoGenerateMips); }
};

struct InputTextureFormat {
  uint8_t pixelSize;
  size_t numComponents;
};

/// <div rustbindgen opaque></div>
struct TextureContextData : public ContextData {
  TextureFormat format;
  WGPUTexture texture{};
  WGPUSampler sampler{};
  WGPUExtent3D size{};

  // Only set for externally managed texture views
  WGPUTextureView externalView{};

  ~TextureContextData() { releaseContextDataConditional(); }

  void releaseContextData() override {
    WGPU_SAFE_RELEASE(wgpuTextureRelease, texture);
    WGPU_SAFE_RELEASE(wgpuSamplerRelease, sampler);
  }
};

/// <div rustbindgen opaque></div>
struct TextureDesc {
  TextureFormat format;
  int2 resolution;
  SamplerState samplerState;
  ImmutableSharedBuffer data;

  // Can wrap an already existing texture if this is passed
  // it will not be released when the texture object is destroyed
  std::optional<WGPUTextureView> externalTexture;

  static TextureDesc getDefault();
};

/// <div rustbindgen opaque></div>
struct Texture final : public TWithContextData<TextureContextData> {
private:
  UniqueId id = getNextId();
  TextureDesc desc = TextureDesc::getDefault();
  std::string label;

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

  const std::string &getLabel() const { return label; }
  const TextureDesc &getDesc() const { return desc; }
  ImmutableSharedBuffer getData() const { return desc.data; }
  const TextureFormat &getFormat() const { return desc.format; }
  int2 getResolution() const { return desc.resolution; }

  TexturePtr clone() const;

  UniqueId getId() const { return id; }

  /// <div rustbindgen hide></div>
  static TexturePtr makeRenderAttachment(WGPUTextureFormat format, std::string &&label);

protected:
  void initContextData(Context &context, TextureContextData &contextData);
  void updateContextData(Context &context, TextureContextData &contextData);

  static UniqueId getNextId();
};

} // namespace gfx

#endif // GFX_TEXTURE
