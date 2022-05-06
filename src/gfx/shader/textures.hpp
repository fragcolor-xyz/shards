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
      binding = &layout.bindings.emplace_back();
      binding->name = name;
    } else {
      binding = &layout.bindings[it->second];
    }
    binding->defaultTexcoordBinding = defaultTexcoordBinding;
  }

  TextureBindingLayout &&finalize() { return std::move(layout); }
};
} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_TEXTURES
