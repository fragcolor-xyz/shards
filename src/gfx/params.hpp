#ifndef GFX_PARAMS
#define GFX_PARAMS

#include "fwd.hpp"
#include "linalg.hpp"
#include "shader/types.hpp"
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <variant>

namespace gfx {

/// <div rustbindgen opaque></div>
typedef std::variant<std::monostate, float, float2, float3, float4, float4x4, uint32_t> ParamVariant;

/// <div rustbindgen opaque></div>
typedef std::variant<std::monostate, float, float2, float3, float4, float4x4, uint32_t> TextureVariant;

/// <div rustbindgen hide></div>
struct TextureParameter {
  TexturePtr texture;
  uint8_t defaultTexcoordBinding = 0;

  TextureParameter(const TexturePtr &texture, uint8_t defaultTexcoordBinding = 0)
      : texture(texture), defaultTexcoordBinding(defaultTexcoordBinding) {}
};

/// <div rustbindgen hide></div>
size_t packParamVariant(uint8_t *outData, size_t outLength, const ParamVariant &variant);

/// <div rustbindgen hide></div>
gfx::shader::FieldType getParamVariantType(const ParamVariant &variant);

/// <div rustbindgen hide></div>
struct IParameterCollector {
  virtual void setParam(const std::string &name, ParamVariant &&value) = 0;
  void setParam(const std::string &name, const ParamVariant &value) { setParam(name, ParamVariant(value)); }

  virtual void setTexture(const std::string &name, TextureParameter &&value) = 0;
  void setTexture(const std::string &name, const TextureParameter &value) { setTexture(name, TextureParameter(value)); }
};

/// <div rustbindgen hide></div>
struct ParameterStorage : public IParameterCollector {
  std::unordered_map<std::string, ParamVariant> data;
  std::unordered_map<std::string, TextureParameter> textures;

  using IParameterCollector::setParam;
  using IParameterCollector::setTexture;
  void setParam(const std::string &name, ParamVariant &&value) { data.insert_or_assign(name, std::move(value)); }
  void setTexture(const std::string &name, TextureParameter &&value) { textures.insert_or_assign(name, std::move(value)); }

  void append(const ParameterStorage &other) {
    for (auto &it : other.data) {
      data.insert_or_assign(it.first, it.second);
    }
  }
};

} // namespace gfx

#endif // GFX_PARAMS
