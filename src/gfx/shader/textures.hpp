#ifndef GFX_SHADER_TEXTURES
#define GFX_SHADER_TEXTURES

#include "fwd.hpp"
#include <map>
#include <vector>

namespace gfx {
namespace shader {

struct TextureBinding {
  String name;
  size_t defaultTexcoordBinding{};
  size_t binding{};
  size_t defaultSamplerBinding{};

  TextureBinding() = default;
  TextureBinding(String name, size_t defaultTexcoordBinding = 0) : name(name), defaultTexcoordBinding(defaultTexcoordBinding) {}
};

struct TextureBindingLayout {
  std::vector<TextureBinding> bindings;
};

struct TextureBindingLayoutBuilder {
private:
  std::map<String, size_t> mapping;
  TextureBindingLayout layout;

public:
  void addOrUpdateSlot(const String &name, size_t defaultTexcoordBinding) {
    auto it = mapping.find(name);
    TextureBinding *binding;
    if (it == mapping.end()) {
      size_t index = layout.bindings.size();
      mapping.insert_or_assign(name, index);

      binding = &layout.bindings.emplace_back();
      binding->name = name;
    } else {
      binding = &layout.bindings[it->second];
    }
    binding->defaultTexcoordBinding = defaultTexcoordBinding;
  }

  void tryUpdateSlot(const String &name, size_t defaultTexcoordBinding) {
    auto it = mapping.find(name);
    if (it != mapping.end()) {
      auto &binding = layout.bindings[it->second];
      binding.defaultTexcoordBinding = defaultTexcoordBinding;
    }
  }

  TextureBindingLayout &&finalize(size_t startBindingIndex, size_t *outBindingIndex = nullptr) {
    for (auto &binding : layout.bindings) {
      binding.binding = startBindingIndex++;
      binding.defaultSamplerBinding = startBindingIndex++;
    }
    if (outBindingIndex)
      *outBindingIndex = startBindingIndex;
    return std::move(layout);
  }
};
} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_TEXTURES
