#ifndef GFX_SHADER_TEXTURES
#define GFX_SHADER_TEXTURES

#include "fwd.hpp"
#include "../error_utils.hpp"
#include "../texture.hpp"
#include "magic_enum.hpp"
#include "fmt.hpp"
#include <map>
#include <vector>

namespace gfx {
namespace shader {

struct TextureBinding {
  FastString name;
  size_t defaultTexcoordBinding{};
  size_t binding{};
  size_t defaultSamplerBinding{};
  TextureType type;

  // TextureBinding() = default;
  TextureBinding(FastString name, TextureType type, size_t defaultTexcoordBinding = 0)
      : name(name), defaultTexcoordBinding(defaultTexcoordBinding), type(type) {}
};

struct TextureBindingLayout {
  std::vector<TextureBinding> bindings;
};

struct TextureBindingLayoutBuilder {
private:
  std::map<FastString, size_t> mapping;
  TextureBindingLayout layout;

public:
  void addOrUpdateSlot(FastString name, TextureType type, size_t defaultTexcoordBinding) {
    auto it = mapping.find(name);
    TextureBinding *binding;
    if (it == mapping.end()) {
      size_t index = layout.bindings.size();
      mapping.insert_or_assign(name, index);

      binding = &layout.bindings.emplace_back(name, type);
    } else {
      binding = &layout.bindings[it->second];
      if (binding->type != type)
        throw formatException("Texture {} redefined as type {} but already defined as {}", name, Type(type),
                              Type(binding->type));
    }
    binding->defaultTexcoordBinding = defaultTexcoordBinding;
  }

  void tryUpdateSlot(FastString name, size_t defaultTexcoordBinding) {
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
