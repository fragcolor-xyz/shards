#ifndef EA04BCE8_29FE_47F5_91CE_D957B325C986
#define EA04BCE8_29FE_47F5_91CE_D957B325C986

#include "../error_utils.hpp"
#include "../math.hpp"
#include "../enums.hpp"
#include "fwd.hpp"
#include "types.hpp"
#include <linalg/linalg.h>
#include <boost/container/flat_map.hpp>
#include <cassert>
#include <map>
#include <vector>
#include <boost/container/small_vector.hpp>
#include <boost/core/span.hpp>

namespace gfx {
namespace shader {

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
  bool isRuntimeSized{};

  StructLayout() = default;
  StructLayout(const StructLayout &other) = default;
  StructLayout(StructLayout &&other) = default;
  StructLayout &operator=(StructLayout &&other) = default;
  StructLayout &operator=(const StructLayout &other) = default;

  // Get the size of this structure inside an array
  size_t getArrayStride() const { return alignTo(size, maxAlignment); }
};

using StructLayoutLookup = boost::container::flat_map<StructType, StructLayout>;

struct StructLayoutBuilder {
private:
  std::map<FastString, size_t> mapping;
  StructLayout bufferLayout;
  size_t offset = 0;
  AddressSpace addressSpace;

  StructLayoutLookup internalLayoutLookup;
  StructLayoutLookup &layoutLookup;

public:
  StructLayoutBuilder(AddressSpace addressSpace) : addressSpace(addressSpace), layoutLookup(internalLayoutLookup) {}
  StructLayoutBuilder(AddressSpace addressSpace, StructLayoutLookup &sll) : addressSpace(addressSpace), layoutLookup(sll) {}

  const StructLayoutLookup &getStructMap() { return layoutLookup; }

  StructLayoutItem generateNext(const Type &paramType) {
    checkNoRuntimeSizedArray();

    StructLayoutItem result;

    size_t elementAlignment = mapAlignment(paramType);
    size_t alignedOffset = alignTo(offset, elementAlignment);

    result.offset = alignedOffset;
    result.size = mapSize(paramType);
    result.type = paramType;

    return result;
  }

  const StructLayoutItem &push(FastString name, const Type &paramType) { return pushInternal(name, generateNext(paramType)); }

  void pushFromStruct(const StructType &root) {
    for (auto &fld : root->entries) {
      push(fld.name, fld.type);
    }
  }

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

  std::optional<std::reference_wrapper<const StructLayout>> findInnerType(const StructType &type) {
    auto it = layoutLookup.find(type);
    if (it == layoutLookup.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  size_t mapSize(const Type &type) {
    return std::visit([this](auto &&arg) -> size_t { return _mapSize(arg); }, type);
  }

  size_t mapAlignment(const Type &type) {
    return std::visit([this](auto &&arg) -> size_t { return _mapAlignment(arg); }, type);
  }

  size_t mapArrayStride(const ArrayType &type) {
    size_t elementAlign = mapAlignment(type->elementType);
    if (addressSpace == AddressSpace::Uniform) {
      // Uniform address space requires that all array elements are aligned to 16 bytes
      elementAlign = roundUpAlignment(elementAlign, 16u);
    }

    size_t stride = alignTo(mapSize(type->elementType), elementAlign);
    return stride;
  }

  size_t _mapSize(const NumType &type) { return type.getByteSize(); }
  size_t _mapSize(const ArrayType &type) {
    size_t stride = mapArrayStride(type);

    if (!type->fixedLength) {
      bufferLayout.isRuntimeSized = true;
      return stride;
    } else {
      return type->fixedLength.value() * stride;
    }
  }
  size_t _mapSize(const StructType &type) {
    auto inner = getInnerStructData(type);
    if (inner.isRuntimeSized) {
      bufferLayout.isRuntimeSized = true;
    }
    return inner.size;
  }
  size_t _mapSize(const TextureType &type) { throw std::runtime_error("Unsupported type"); }
  size_t _mapSize(const SamplerType &type) { throw std::runtime_error("Unsupported type"); }

  size_t _mapAlignment(const NumType &type) { return type.getWGSLAlignment(); }
  size_t _mapAlignment(const ArrayType &type) {
    size_t align = mapAlignment(type->elementType);
    if (addressSpace == AddressSpace::Uniform) {
      align = roundUpAlignment(align, 16u);
    }
    return align;
  }
  size_t _mapAlignment(const StructType &type) { return getInnerStructData(type).maxAlignment; }
  size_t _mapAlignment(const TextureType &type) { throw std::runtime_error("Unsupported type"); }
  size_t _mapAlignment(const SamplerType &type) { throw std::runtime_error("Unsupported type"); }

private:
  void checkNoRuntimeSizedArray() {
    if (bufferLayout.isRuntimeSized)
      throw std::runtime_error("Can not field after a runtime sized array, it needs to be the last item");
  }

  void updateOutput() {
    if (addressSpace == AddressSpace::Uniform) {
      bufferLayout.maxAlignment = roundUpAlignment(bufferLayout.maxAlignment, 16u);
    }
    bufferLayout.size = alignTo(offset, bufferLayout.maxAlignment);
  }

  StructLayout processInnerStruct(const StructType &st) const {
    StructLayoutBuilder lb(addressSpace);
    lb.pushFromStruct(st);
    return lb.finalize();
  }

  const StructLayout &getInnerStructData(const StructType &st) {
    auto it = layoutLookup.find(st);
    if (it == layoutLookup.end()) {
      auto layout = processInnerStruct(st);
      it = layoutLookup.insert_or_assign(st, layout).first;
    }
    return it->second;
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
    size_t fieldAlignment = mapAlignment(pushedElement.type);

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

struct LayoutPath {
  boost::container::small_vector<FastString, 12> path;

  LayoutPath() = default;
  LayoutPath(boost::span<const FastString> components) : path(components.begin(), components.end()) {}
  LayoutPath(std::initializer_list<FastString> components) : path(components) {}

  void push_back(FastString str) { path.push_back(str); }
  void clear() { path.clear(); }
  bool empty() const { return path.empty(); }

  LayoutPath next(size_t skip = 1) const {
    if (skip > path.size())
      return LayoutPath();
    auto span = boost::span(path);
    return LayoutPath(span.subspan(skip));
  }

  operator bool() const { return !empty(); }
  std::strong_ordering operator<=>(const LayoutPath &other) const = default;

  // Gets the head path component
  FastString getHead() const {
    shassert(path.size() > 0);
    return path.front();
  }
};

struct LayoutRef {
  LayoutPath path;
  size_t offset{};
  size_t size{};
  Type type{};
};

struct LayoutTraverser {
  const StructLayout &base;
  const StructLayoutLookup &structLookup;
  LayoutTraverser(const StructLayout &base, const StructLayoutLookup &structLookup) : base(base), structLookup(structLookup) {}

  LayoutRef findRuntimeSizedArray() {
    LayoutRef ref;
    if (findRuntimeSizedField(ref, base))
      return ref;
    throw std::runtime_error("Struct doesn't have a runtime sized array");
  }

  std::optional<LayoutRef> find(const LayoutPath &p) {
    const StructLayout *slNode = &base;
    const StructLayoutItem *lastItem{};
    LayoutRef r{.path = p};
    LayoutPath next = p;
    while (next) {
      if (!slNode) {
        // The type visited is not a struct
        return std::nullopt;
      }

      auto head = next.getHead();
      auto it = std::find_if(slNode->fieldNames.begin(), slNode->fieldNames.end(),
                             [&head](const FastString &str) { return str == head; });
      if (it == slNode->fieldNames.end()) {
        return std::nullopt;
      }

      size_t childIndex = it - slNode->fieldNames.begin();

      lastItem = &slNode->items[childIndex];

      r.offset += lastItem->offset;

      if (auto st = std::get_if<StructType>(&lastItem->type)) {
        slNode = &structLookup.at(*st);
      } else {
        slNode = nullptr;
      }

      next = next.next();
    }

    if (!lastItem)
      throw std::runtime_error("Invalid reference");
    r.type = lastItem->type;
    r.size = lastItem->size;

    return r;
  }

private:
  bool findRuntimeSizedField(LayoutRef &r, const StructLayout &sl) {
    if (sl.fieldNames.empty())
      return false;
    auto &last = sl.items.back();
    if (std::get_if<ArrayType>(&last.type)) {
      r.path.push_back(sl.fieldNames.back());
      r.offset += last.offset;
      r.size = last.size;
      r.type = last.type;
      return true;
    } else if (auto st = std::get_if<StructType>(&last.type)) {
      r.offset += last.offset;
      return findRuntimeSizedField(r, structLookup.at(*st));
    }
    return false;
  }
};

inline size_t runtimeBufferSizeHelper(const StructLayout &layout, const LayoutRef &runtimeSizedField, size_t runtimeLength) {
  size_t rawSize = runtimeSizedField.offset + runtimeSizedField.size * runtimeLength;
  return alignTo(rawSize, layout.maxAlignment);
}

} // namespace shader
} // namespace gfx

#endif /* EA04BCE8_29FE_47F5_91CE_D957B325C986 */
