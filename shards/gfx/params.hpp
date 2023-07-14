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
};

/// <div rustbindgen hide></div>
size_t packNumParameter(uint8_t *outData, size_t outLength, const NumParameter &variant);

/// <div rustbindgen hide></div>
gfx::shader::NumFieldType getNumParameterType(const NumParameter &variant);

/// <div rustbindgen hide></div>
struct IParameterCollector {
  virtual void setParam(const char *name, NumParameter &&value) = 0;
  virtual void setTexture(const char *name, TextureParameter &&value) = 0;

  void setParam(const std::string &name, const NumParameter &value) { setParam(name.c_str(), NumParameter(value)); }
  void setTexture(const std::string &name, const TextureParameter &value) { setTexture(name.c_str(), TextureParameter(value)); }
};

using ShaderParameter = std::variant<NumParameter, TextureParameter>;

} // namespace gfx

#endif // GFX_PARAMS
