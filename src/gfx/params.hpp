#pragma once
#include "fwd.hpp"
#include "linalg.hpp"
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <variant>

namespace gfx {

enum class ShaderParamType : uint8_t {
#define PARAM_TYPE(_cppType, _displayName, ...) _displayName,
#include "param_types.def"
#undef PARAM_TYPE
};

typedef std::variant<std::monostate, float, float2, float3, float4, float4x4> ParamVariant;

struct TextureParameter {
  TexturePtr texture;
  uint8_t defaultTexcoordBinding = 0;

  TextureParameter(const TexturePtr &texture, uint8_t defaultTexcoordBinding = 0)
      : texture(texture), defaultTexcoordBinding(defaultTexcoordBinding) {}
};

size_t packParamVariant(uint8_t *outData, size_t outLength, const ParamVariant &variant);
ShaderParamType getParamVariantType(const ParamVariant &variant);
size_t getParamTypeSize(ShaderParamType type);
size_t getParamTypeWGSLAlignment(ShaderParamType type);

struct IDrawDataCollector {
  virtual void setParam(const std::string &name, ParamVariant &&value) = 0;
  void setParam(const std::string &name, const ParamVariant &value) { setParam(name, ParamVariant(value)); }

  virtual void setTexture(const std::string &name, TextureParameter &&value) = 0;
  void setTexture(const std::string &name, const TextureParameter &value) { setTexture(name, TextureParameter(value)); }
};

struct DrawData : public IDrawDataCollector {
  std::unordered_map<std::string, ParamVariant> data;
  std::unordered_map<std::string, TextureParameter> textures;

  using IDrawDataCollector::setParam;
  using IDrawDataCollector::setTexture;
  void setParam(const std::string &name, ParamVariant &&value) { data.insert_or_assign(name, std::move(value)); }
  void setTexture(const std::string &name, TextureParameter &&value) { textures.insert_or_assign(name, std::move(value)); }

  void append(const DrawData &other) {
    for (auto &it : other.data) {
      data.insert_or_assign(it.first, it.second);
    }
  }
};

} // namespace gfx
