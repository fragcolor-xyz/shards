#ifndef GFX_SHADER_UNIFORMS
#define GFX_SHADER_UNIFORMS

#include "../error_utils.hpp"
#include "../math.hpp"
#include "fwd.hpp"
#include "types.hpp"
#include <cassert>
#include <map>
#include <vector>

namespace gfx {
namespace shader {
struct UniformLayout {
  size_t offset{};
  size_t size{};
  FieldType type{};
};

struct UniformBufferLayout {
  std::vector<UniformLayout> items;
  std::vector<String> fieldNames;
  size_t size = ~0;
  size_t maxAlignment = 0;

  UniformBufferLayout() = default;
  UniformBufferLayout(const UniformBufferLayout &other) = default;
  UniformBufferLayout(UniformBufferLayout &&other) = default;
  UniformBufferLayout &operator=(UniformBufferLayout &&other) = default;
  UniformBufferLayout &operator=(const UniformBufferLayout &other) = default;
};

struct UniformBufferLayoutBuilder {
private:
  std::map<String, size_t> mapping;
  UniformBufferLayout bufferLayout;
  size_t offset = 0;

public:
  UniformLayout generateNext(const FieldType &paramType) const {
    UniformLayout result;

    size_t elementAlignment = paramType.getWGSLAlignment();
    size_t alignedOffset = alignTo(offset, elementAlignment);

    result.offset = alignedOffset;
    result.size = paramType.getByteSize();
    result.type = paramType;
    return result;
  }

  const UniformLayout &push(const String &name, const FieldType &paramType) {
    return pushInternal(name, generateNext(paramType));
  }

  UniformBufferLayout &&finalize() {
    bufferLayout.size = offset;
    return std::move(bufferLayout);
  }

private:
  UniformLayout &pushInternal(const String &name, UniformLayout &&layout) {
    auto itExisting = mapping.find(name);
    if (itExisting != mapping.end()) {
      throw formatException("Uniform layout has duplicate field {}", name);
    }

    size_t index = mapping.size();
    bufferLayout.fieldNames.push_back(name);
    bufferLayout.items.emplace_back(std::move(layout));
    mapping.insert_or_assign(name, index);

    UniformLayout &result = bufferLayout.items.back();

    offset = std::max(offset, result.offset + result.size);
    size_t fieldAlignment = result.type.getWGSLAlignment();

    // Update struct alignment to the max of it's members
    // https://www.w3.org/TR/WGSL/#alignment-and-size
    bufferLayout.maxAlignment = std::max(bufferLayout.maxAlignment, fieldAlignment);

    return result;
  }
};

struct BufferDefinition {
  String variableName;
  UniformBufferLayout layout;
  std::optional<String> indexedBy;
};

struct TextureDefinition {
  String variableName;
  String defaultTexcoordVariableName;
  String defaultSamplerVariableName;
};

} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_UNIFORMS
