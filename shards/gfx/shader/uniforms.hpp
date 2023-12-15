#ifndef GFX_SHADER_UNIFORMS
#define GFX_SHADER_UNIFORMS

#include "../error_utils.hpp"
#include "../math.hpp"
#include "../enums.hpp"
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
  NumFieldType type{};

  bool isEqualIgnoreOffset(const UniformLayout &other) const { return size == other.size && type == other.type; }
};

struct UniformBufferLayout {
  std::vector<UniformLayout> items;
  std::vector<FastString> fieldNames;
  size_t size = ~0;
  size_t maxAlignment = 0;

  UniformBufferLayout() = default;
  UniformBufferLayout(const UniformBufferLayout &other) = default;
  UniformBufferLayout(UniformBufferLayout &&other) = default;
  UniformBufferLayout &operator=(UniformBufferLayout &&other) = default;
  UniformBufferLayout &operator=(const UniformBufferLayout &other) = default;

  // Get the size of this structure inside an array
  size_t getArrayStride() const { return alignTo(size, maxAlignment); }
};

struct UniformBufferLayoutBuilder {
private:
  std::map<FastString, size_t> mapping;
  UniformBufferLayout bufferLayout;
  size_t offset = 0;

public:
  UniformLayout generateNext(const NumFieldType &paramType) const {
    UniformLayout result;

    size_t elementAlignment = paramType.getWGSLAlignment();
    size_t alignedOffset = alignTo(offset, elementAlignment);

    result.offset = alignedOffset;
    result.size = paramType.getByteSize();
    result.type = paramType;
    return result;
  }

  const UniformLayout &push(FastString name, const NumFieldType &paramType) {
    return pushInternal(name, generateNext(paramType));
  }

  // Removes fields that do not pass the filter(name, UniformLayout)
  template <typename T> void optimize(T &&filter) {
    struct QueueItem {
      FastString name;
      NumFieldType fieldType;
    };
    std::vector<QueueItem> queue;

    // Collect name & type pairs to keep
    for (auto it = mapping.begin(); it != mapping.end(); it++) {
      FastString name = bufferLayout.fieldNames[it->second];
      const UniformLayout &layout = bufferLayout.items[it->second];

      if (filter(name, layout)) {
        auto &element = queue.emplace_back();
        element.name = name;
        element.fieldType = layout.type;
      }
    }

    // Rebuild the layout
    offset = 0;
    mapping.clear();
    bufferLayout = UniformBufferLayout{};
    for (auto &queueItem : queue) {
      push(queueItem.name, queueItem.fieldType);
    }
  }

  // Returns the layout as it is currently
  const UniformBufferLayout &getCurrentFinalLayout() {
    bufferLayout.size = offset;
    return bufferLayout;
  }

  // Finalizes the layout and moves out the result.
  // WARNING: do not reuse this builder after calling finalize
  UniformBufferLayout &&finalize() {
    bufferLayout.size = offset;
    return std::move(bufferLayout);
  }

private:
  UniformLayout &pushInternal(FastString name, UniformLayout &&layout) {
    auto itExisting = mapping.find(name);
    if (itExisting != mapping.end()) {
      auto &existingLayout = bufferLayout.items[itExisting->second];
      if (existingLayout.isEqualIgnoreOffset(layout)) {
        return existingLayout;
      }

      throw formatException("Uniform layout has duplicate field {} with a different type", name);
    }

    size_t index = mapping.size();
    bufferLayout.fieldNames.push_back(name);
    bufferLayout.items.emplace_back(std::move(layout));
    mapping.insert_or_assign(name, index);

    UniformLayout &result = bufferLayout.items.back();

    updateOffsetAndMaxAlign(result);

    return result;
  }

  void updateOffsetAndMaxAlign(const UniformLayout &pushedElement) {
    offset = std::max(offset, pushedElement.offset + pushedElement.size);
    size_t fieldAlignment = pushedElement.type.getWGSLAlignment();

    // Update struct alignment to the max of it's members
    // https://www.w3.org/TR/WGSL/#alignment-and-size
    bufferLayout.maxAlignment = std::max(bufferLayout.maxAlignment, fieldAlignment);
  }
};

namespace dim {
// One single data structure
struct One {};
// A dynamically sized array of the data structure, which is indexed based on the draw instance index
struct PerInstance {};
// A dynamically sized array
struct Dynamic {};
// A fixed size array structures
struct Fixed {
  size_t length;
  Fixed(size_t length) : length(length) {}
};
}; // namespace dim
using Dimension = std::variant<dim::One, dim::PerInstance, dim::Dynamic, dim::Fixed>;

struct BufferDefinition {
  FastString variableName;
  UniformBufferLayout layout;
  Dimension dimension = dim::One{};

  inline const UniformLayout *findField(const char *fieldName) const {
    for (size_t i = 0; i < layout.fieldNames.size(); i++) {
      if (layout.fieldNames[i] == fieldName) {
        return &layout.items[i];
      }
    }
    return nullptr;
  }
};

struct TextureDefinition {
  FastString variableName;
  FastString defaultTexcoordVariableName;
  FastString defaultSamplerVariableName;
  TextureFieldType type;

  TextureDefinition(FastString variableName) : variableName(variableName) {}
  TextureDefinition(FastString variableName, FastString defaultTexcoordVariableName, FastString defaultSamplerVariableName,
                    TextureFieldType type)
      : variableName(variableName), defaultTexcoordVariableName(defaultTexcoordVariableName),
        defaultSamplerVariableName(defaultSamplerVariableName), type(type) {}
};

} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_UNIFORMS
