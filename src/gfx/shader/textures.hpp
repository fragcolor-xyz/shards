#ifndef GFX_SHADER_TEXTURES
#define GFX_SHADER_TEXTURES

#include "fwd.hpp"
#include "../error_utils.hpp"
#include "../texture.hpp"
#include "magic_enum.hpp"
#include <map>
#include <vector>

namespace gfx {
namespace shader {

struct TextureBinding {
  String name;
  size_t defaultTexcoordBinding{};
  size_t binding{};
  size_t defaultSamplerBinding{};
  TextureDimension dimension;

  // TextureBinding() = default;
  TextureBinding(String name, TextureDimension dimension, size_t defaultTexcoordBinding = 0)
      : name(name), defaultTexcoordBinding(defaultTexcoordBinding), dimension(dimension) {}
};

struct TextureBindingLayout {
  std::vector<TextureBinding> bindings;
};

struct TextureBindingLayoutBuilder {
private:
  std::map<String, size_t> mapping;
  TextureBindingLayout layout;

public:
  void addOrUpdateSlot(const String &name, TextureDimension dimension, size_t defaultTexcoordBinding) {
    auto it = mapping.find(name);
    TextureBinding *binding;
    if (it == mapping.end()) {
      size_t index = layout.bindings.size();
      mapping.insert_or_assign(name, index);

      binding = &layout.bindings.emplace_back(name, dimension);
    } else {
      binding = &layout.bindings[it->second];
      if (binding->dimension != dimension)
        throw formatException("Texture {} redefined as type {} but already defined as {}", name, magic_enum::enum_name(dimension),
                              magic_enum::enum_name(binding->dimension));
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

  const TextureBindingLayout &getCurrentFinalLayout(size_t startBindingIndex, size_t *outBindingIndex = nullptr) {
    prepareFinalLayout(startBindingIndex, outBindingIndex);
    return layout;
  }

  // Finishes and moves out the resulting layout.
  // WARNING: Do not reuse this builder after calling finalize
  TextureBindingLayout &&finalize(size_t startBindingIndex, size_t *outBindingIndex = nullptr) {
    prepareFinalLayout(startBindingIndex, outBindingIndex);
    return std::move(layout);
  }

private:
  void prepareFinalLayout(size_t startBindingIndex, size_t *outBindingIndex = nullptr) {
    for (auto &binding : layout.bindings) {
      binding.binding = startBindingIndex++;
      binding.defaultSamplerBinding = startBindingIndex++;
    }
    if (outBindingIndex)
      *outBindingIndex = startBindingIndex;
  }
};
} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_TEXTURES
