#ifndef EA04BCE8_29FE_47F5_91CE_D957B325C986
#define EA04BCE8_29FE_47F5_91CE_D957B325C986

#include "../error_utils.hpp"
#include "../math.hpp"
#include "../enums.hpp"
#include "fwd.hpp"
#include "types.hpp"
#include <boost/container/flat_map.hpp>
#include <cassert>
#include <map>
#include <vector>

namespace gfx {
namespace shader {

enum class AddressSpace {
  Uniform,
  Storage,
  StorageRW,
};

struct StructLayoutItem {
  size_t offset{};
  size_t size{};
  Type type{};

  bool isEqualIgnoreOffset(const StructLayoutItem &other) const { return size == other.size && type == other.type; }
};

struct StructLayout {
  std::vector<StructLayoutItem> items;
  std::vector<FastString> fieldNames;
  size_t size = ~0;
  size_t maxAlignment = 0;

  StructLayout() = default;
  StructLayout(const StructLayout &other) = default;
  StructLayout(StructLayout &&other) = default;
  StructLayout &operator=(StructLayout &&other) = default;
  StructLayout &operator=(const StructLayout &other) = default;

  // Get the size of this structure inside an array
  size_t getArrayStride() const { return alignTo(size, maxAlignment); }
};

struct StructLayoutBuilder {
private:
  std::map<FastString, size_t> mapping;
  StructLayout bufferLayout;
  size_t offset = 0;
  AddressSpace addressSpace;

  struct CachedStructData {};
  boost::container::flat_map<StructType, CachedStructData> structCache;

public:
  StructLayoutBuilder(AddressSpace addressSpace) : addressSpace(addressSpace) {}

  StructLayoutItem generateNext(const Type &paramType) const {
    StructLayoutItem result;

    size_t elementAlignment = getWGSLAlignment(paramType);
    size_t alignedOffset = alignTo(offset, elementAlignment);

    result.offset = alignedOffset;
    result.size = getByteSize(paramType);
    result.type = paramType;
    return result;
  }

  const StructLayoutItem &push(FastString name, const Type &paramType) { return pushInternal(name, generateNext(paramType)); }

  // Removes fields that do not pass the filter(name, LayoutItem)
  template <typename T> void optimize(T &&filter) {
    struct QueueItem {
      FastString name;
      NumType fieldType;
    };
    std::vector<QueueItem> queue;

    // Collect name & type pairs to keep
    for (auto it = mapping.begin(); it != mapping.end(); it++) {
      FastString name = bufferLayout.fieldNames[it->second];
      const StructLayoutItem &layout = bufferLayout.items[it->second];

      if (filter(name, layout)) {
        auto &element = queue.emplace_back();
        element.name = name;
        element.fieldType = layout.type;
      }
    }

    // Rebuild the layout
    offset = 0;
    mapping.clear();
    bufferLayout = StructLayout{};
    for (auto &queueItem : queue) {
      push(queueItem.name, queueItem.fieldType);
    }
  }

  std::optional<std::reference_wrapper<const StructLayoutItem>> forceAlignmentTo(size_t alignment) {
    auto &tmpLayout = getCurrentFinalLayout();
    size_t structSize = tmpLayout.getArrayStride();
    size_t alignedSize = alignTo(structSize, alignment);
    shassert(alignTo<4>(alignedSize) == alignedSize); // Check multiple of 4
    size_t size4ToPad = (alignedSize - structSize) / 4;
    if (size4ToPad > 0) {
      return push("_array_padding_", ArrayType(Types::Float, size4ToPad));
    }
    return std::nullopt;
  }

  // Returns the layout as it is currently
  const StructLayout &getCurrentFinalLayout() {
    updateOutput();
    return bufferLayout;
  }

  // Finalizes the layout and moves out the result.
  // WARNING: do not reuse this builder after calling finalize
  StructLayout &&finalize() {
    updateOutput();
    return std::move(bufferLayout);
  }

private:
  void updateOutput() {
    if (addressSpace == AddressSpace::Uniform) {
      bufferLayout.maxAlignment = roundUpAlignment(bufferLayout.maxAlignment, 16u);
    }
    bufferLayout.size = alignTo(offset, bufferLayout.maxAlignment);
  }

  size_t _getByteSize(const NumType &type) const { return type.getByteSize(); }
  size_t _getByteSize(const ArrayType &type) const {
    if (!type->fixedLength) {
      throw std::runtime_error("Can not determine size of runtime-sized array");
    }
    size_t elementAlign = getWGSLAlignment(type->elementType);
    if (addressSpace == AddressSpace::Uniform) {
      elementAlign = roundUpAlignment(elementAlign, 16u);
    }
    return type->fixedLength.value() * alignTo(getByteSize(type->elementType), elementAlign);
  }
  size_t _getByteSize(const StructType &type) const { throw std::runtime_error("Unsupported type"); }
  size_t _getByteSize(const TextureType &type) const { throw std::runtime_error("Unsupported type"); }
  size_t _getByteSize(const SamplerType &type) const { throw std::runtime_error("Unsupported type"); }
  size_t getByteSize(const Type &type) const {
    return std::visit([this](auto &&arg) -> size_t { return _getByteSize(arg); }, type);
  }

  size_t _getWGSLAlignment(const NumType &type) const { return type.getWGSLAlignment(); }
  size_t _getWGSLAlignment(const ArrayType &type) const {
    size_t align = getWGSLAlignment(type->elementType);
    if (addressSpace == AddressSpace::Uniform) {
      align = roundUpAlignment(align, 16u);
    }
    return align;
  }
  size_t _getWGSLAlignment(const StructType &type) const { throw std::runtime_error("Unsupported type"); }
  size_t _getWGSLAlignment(const TextureType &type) const { throw std::runtime_error("Unsupported type"); }
  size_t _getWGSLAlignment(const SamplerType &type) const { throw std::runtime_error("Unsupported type"); }
  size_t getWGSLAlignment(const Type &type) const {
    return std::visit([this](auto &&arg) -> size_t { return _getWGSLAlignment(arg); }, type);
  }

  StructLayoutItem &pushInternal(FastString name, StructLayoutItem &&layout) {
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

    StructLayoutItem &result = bufferLayout.items.back();

    updateOffsetAndMaxAlign(result);

    return result;
  }

  void updateOffsetAndMaxAlign(const StructLayoutItem &pushedElement) {
    offset = std::max(offset, pushedElement.offset + pushedElement.size);
    size_t fieldAlignment = getWGSLAlignment(pushedElement.type);

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
  Type layout;
  Dimension dimension = dim::One{};

  inline const StructField *findField(FastString fieldName) const {
    const StructType *structType = std::get_if<StructType>(&layout);
    if (!structType) {
      return nullptr;
    }

    const auto &e = (*structType)->entries;
    for (size_t i = 0; i < e.size(); i++) {
      if (e[i].name == fieldName) {
        return &e[i];
      }
    }
    return nullptr;
  }
};

struct TextureDefinition {
  FastString variableName;
  FastString defaultTexcoordVariableName;
  FastString defaultSamplerVariableName;
  TextureType type;

  TextureDefinition(FastString variableName) : variableName(variableName) {}
  TextureDefinition(FastString variableName, FastString defaultTexcoordVariableName, FastString defaultSamplerVariableName,
                    TextureType type)
      : variableName(variableName), defaultTexcoordVariableName(defaultTexcoordVariableName),
        defaultSamplerVariableName(defaultSamplerVariableName), type(type) {}
};

} // namespace shader
} // namespace gfx

#endif /* EA04BCE8_29FE_47F5_91CE_D957B325C986 */
