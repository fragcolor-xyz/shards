#ifndef E5B10F2C_DC03_4AD7_A9C1_C507688C15D2
#define E5B10F2C_DC03_4AD7_A9C1_C507688C15D2

#include "context_data.hpp"
#include "context.hpp"
#include "gfx_wgpu.hpp"
#include "linalg.hpp"
#include "texture.hpp"
#include "wgpu_handle.hpp"
#include "fwd.hpp"
#include <compare>
#include <map>

namespace gfx {

namespace RenderTargetSize {
struct MainOutput {
  float2 scale = float2(1, 1);

  int2 apply(int2 referenceSize) const { return (int2)linalg::floor(float2(referenceSize) * scale); }
};

struct Fixed {
  int2 size;

  int2 apply(int2 _referenceSize) const { return size; }
};
}; // namespace RenderTargetSize

/// <div rustbindgen hide></div>
typedef std::variant<RenderTargetSize::MainOutput, RenderTargetSize::Fixed> RenderTargetSizeVariant;

struct TextureSubResource {
  std::shared_ptr<Texture> texture;
  uint8_t faceIndex{};

  TextureSubResource() = default;
  TextureSubResource(std::shared_ptr<Texture> texture, uint8_t face = 0) : texture(texture), faceIndex(face) {}
  TextureSubResource(const TextureSubResource &) = default;

  operator bool() const { return (bool)texture; }
  operator const std::shared_ptr<Texture> &() const { return texture; }

  std::strong_ordering operator<=>(const TextureSubResource &) const = default;
};

// Group of named view textures
/// <div rustbindgen opaque></div>
struct RenderTarget {
  std::map<std::string, TextureSubResource> attachments;
  std::string label;
  RenderTargetSizeVariant size = RenderTargetSize::MainOutput{};

private:
  int2 computedSize{};

public:
  /// <div rustbindgen hide></div>
  RenderTarget(std::string &&label = "unknown") : label(std::move(label)) {}

  /// Configures a new named attachement
  /// <div rustbindgen hide></div>
  void configure(const char *name, WGPUTextureFormat format);

  /// <div rustbindgen hide></div>
  int2 resizeConditional(int2 mainOutputSize);

  /// <div rustbindgen hide></div>
  int2 getSize() const;

  /// <div rustbindgen hide></div>
  int2 computeSize(int2 mainOutputSize) const;

  /// <div rustbindgen hide></div>
  const TextureSubResource &getAttachment(const std::string &name) const;

  const TextureSubResource &operator[](const std::string &name) const { return getAttachment(name); }
};

} // namespace gfx

#endif /* E5B10F2C_DC03_4AD7_A9C1_C507688C15D2 */
