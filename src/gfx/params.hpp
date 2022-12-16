#ifndef GFX_PARAMS
#define GFX_PARAMS

#include "fwd.hpp"
#include "linalg.hpp"
#include "shader/types.hpp"
#include <memory>
#include <stdint.h>
#include <string>
#include <map>
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
  virtual void setParam(const char *name, ParamVariant &&value) = 0;
  virtual void setTexture(const char *name, TextureParameter &&value) = 0;

  void setParam(const std::string &name, const ParamVariant &value) { setParam(name.c_str(), ParamVariant(value)); }
  void setTexture(const std::string &name, const TextureParameter &value) { setTexture(name.c_str(), TextureParameter(value)); }
};

/// <div rustbindgen hide></div>
struct ParameterStorage : public IParameterCollector {
  using allocator_type = std::pmr::polymorphic_allocator<>;

  struct KeyLess {
    using is_transparent = std::true_type;
    template <typename T, typename U> bool operator()(const T &a, const U &b) const {
      return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
    }
  };

  std::pmr::map<std::pmr::string, ParamVariant, KeyLess> data;
  std::pmr::map<std::pmr::string, TextureParameter, KeyLess> textures;

  using IParameterCollector::setParam;
  using IParameterCollector::setTexture;

  ParameterStorage() = default;
  ParameterStorage(allocator_type allocator) : data(allocator), textures(allocator) {}
  ParameterStorage(ParameterStorage &&other, allocator_type allocator) : data(allocator), textures(allocator) {
    *this = std::move(other);
  }
  ParameterStorage &operator=(ParameterStorage &&) = default;

  void setParam(const char *name, ParamVariant &&value) { data.emplace(name, std::move(value)); }
  void setTexture(const char *name, TextureParameter &&value) { textures.emplace(name, std::move(value)); }

  void setParamIfUnset(const std::pmr::string& key, const ParamVariant &value) {
    if (!data.contains(key)) {
      data.emplace(key, value);
    }
  }

  void append(const ParameterStorage &other) {
    for (auto &it : other.data) {
      data.emplace(it);
    }
  }
};

} // namespace gfx

#endif // GFX_PARAMS
