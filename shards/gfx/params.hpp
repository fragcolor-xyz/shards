#ifndef GFX_PARAMS
#define GFX_PARAMS

#include "fwd.hpp"
#include "linalg.hpp"
#include "shader/types.hpp"
#include <memory>
#include <stdint.h>
#include <string>
#include <variant>

namespace gfx {
/// <div rustbindgen opaque></div>
typedef std::variant<std::monostate, float, float2, float3, float4, float4x4, uint32_t, int32_t, int2, int3, int4> NumParameter;

/// <div rustbindgen hide></div>
struct TextureParameter {
  TexturePtr texture;
  uint8_t defaultTexcoordBinding = 0;

  TextureParameter(const TexturePtr &texture, uint8_t defaultTexcoordBinding = 0)
      : texture(texture), defaultTexcoordBinding(defaultTexcoordBinding) {}

  std::strong_ordering operator<=>(const TextureParameter &other) const = default;
};

/// <div rustbindgen hide></div>
size_t packNumParameter(uint8_t *outData, size_t outLength, const NumParameter &variant);

/// <div rustbindgen hide></div>
gfx::shader::NumFieldType getNumParameterType(const NumParameter &variant);

/// <div rustbindgen hide></div>
struct IParameterCollector {
  virtual void setParam(FastString name, NumParameter &&value) = 0;
  virtual void setTexture(FastString name, TextureParameter &&value) = 0;

  void setParam(FastString name, const NumParameter &value) { setParam(name, NumParameter(value)); }
  void setTexture(FastString name, const TextureParameter &value) { setTexture(name, TextureParameter(value)); }
};

} // namespace gfx

#endif // GFX_PARAMS
