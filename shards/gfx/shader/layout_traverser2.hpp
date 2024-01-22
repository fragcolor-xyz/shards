#ifndef C9B5D141_DE0C_4173_97E1_0E5367526CFA
#define C9B5D141_DE0C_4173_97E1_0E5367526CFA

#include "struct_layout.hpp"
#include <tracy/Wrapper.hpp>
#include <boost/tti/has_member_function.hpp>

namespace gfx::shader {
namespace detail {
template <typename T> struct IsReferenceWrapper : std::false_type {};
template <typename T> struct IsReferenceWrapper<std::reference_wrapper<T>> : std::true_type {};
} // namespace detail

struct LayoutTraverser2;
struct LayoutRef2 {
  LayoutTraverser2 &lt;
  size_t offset;
  std::variant<std::reference_wrapper<const StructLayoutItem>, std::reference_wrapper<const StructLayout>, StructLayoutItem> ref;
  // This is set for array elements
  std::optional<size_t> arrayElementStride;

  // Calls the inner function with the reference type regardles of whether it is a reference wrapper or not
  template <typename R, typename F> static R visitUnpackRef(F &&lambda, auto &&arg) {
    return std::visit(
        [&](auto &arg1) -> R {
          using T = std::decay_t<decltype(arg1)>;
          if constexpr (detail::IsReferenceWrapper<T>::value) {
            return lambda((typename T::type &)arg1);
          } else {
            return lambda((T &)arg1);
          }
        },
        arg);
  }

  LayoutRef2 inner() {
    ZoneScopedN("LayoutTraverser2::innerArrayElem");
    return visitUnpackRef<LayoutRef2>(
        [&](auto &arg) -> LayoutRef2 {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, StructLayoutItem>) {
            LayoutRef2 inner = getInnerType(arg);
            inner.offset = arg.offset;
            return inner;
          } else if constexpr (std::is_same_v<T, StructLayout>) {
            throw std::logic_error("Type does not have an inner type");
          }
        },
        ref);
  }

  std::optional<LayoutRef2> inner(FastString key) const {
    ZoneScopedN("LayoutTraverser2::innerStructElem");
    return visitUnpackRef<std::optional<LayoutRef2>>(
        [&](auto &arg) -> std::optional<LayoutRef2> {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, StructLayoutItem>) {
            throw std::logic_error("Can not index type with key, Type is not a struct");
          } else if constexpr (std::is_same_v<T, StructLayout>) {
            auto it =
                std::find_if(arg.fieldNames.begin(), arg.fieldNames.end(), [&](const FastString &str) { return str == key; });
            if (it == arg.fieldNames.end()) {
              return std::nullopt;
            }

            size_t childIndex = it - arg.fieldNames.begin();
            auto &item = arg.items[childIndex];
            return LayoutRef2{
                .lt = lt,
                .offset = offset + item.offset,
                .ref = std::reference_wrapper(item),
            };
          }
        },
        ref);
  }

  size_t size() const {
    return visitUnpackRef<size_t>([&](auto &arg) { return arg.size; }, ref);
  }

  const Type &type() const {
    return visitUnpackRef<const Type &>(
        [&](auto &arg) -> const Type & {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, StructLayoutItem>) {
            return arg.type;
          } else if constexpr (std::is_same_v<T, StructLayout>) {
            throw std::logic_error("Type does not have an inner type");
          }
        },
        ref);
  }

private:
  LayoutRef2 getInnerType(const StructLayoutItem &sli);
};

struct LayoutTraverser2 {
  const StructLayout &base;
  const StructLayoutLookup &structLookup;
  AddressSpace addressSpace;
  LayoutTraverser2(const StructLayout &base, const StructLayoutLookup &structLookup, AddressSpace addressSpace)
      : base(base), structLookup(structLookup), addressSpace(addressSpace) {}

  LayoutRef2 root() { return LayoutRef2{.lt = *this, .ref = base}; }
};

inline LayoutRef2 synthesizeArrayInnerRef(const StructLayoutItem &sli_, const LayoutRef2 &parent, const ArrayType &arrayType,
                                          const Type &elementType) {
  StructLayoutBuilder slb(parent.lt.addressSpace, const_cast<StructLayoutLookup &>(parent.lt.structLookup));
  size_t arrayStride = slb.mapArrayStride(arrayType);

  return std::visit(
      [&](auto &&arg1) -> LayoutRef2 {
        using T1 = std::decay_t<decltype(arg1)>;
        if constexpr (std::is_same_v<T1, StructType>) {
          return LayoutRef2{
              .lt = parent.lt,
              .offset = parent.offset,
              .ref = std::reference_wrapper(parent.lt.structLookup.at(arg1)),
              .arrayElementStride = arrayStride,
          };
        } else if constexpr (std::is_same_v<T1, ArrayType>) {
          StructLayoutItem sli = sli_;
          sli.size = slb._mapSize(arg1);
          sli.type = elementType;
          return LayoutRef2{.lt = parent.lt, .offset = parent.offset, .ref = sli, .arrayElementStride = arrayStride};
        } else if constexpr (std::is_same_v<T1, NumType>) {
          StructLayoutItem sli = sli_;
          sli.size = arg1.getByteSize();
          sli.type = elementType;
          return LayoutRef2{.lt = parent.lt, .offset = parent.offset, .ref = sli, .arrayElementStride = arrayStride};
        } else {
          throw std::logic_error(fmt::format("Invalid array element type: {}", elementType));
        }
      },
      elementType);
}

inline LayoutRef2 LayoutRef2::getInnerType(const StructLayoutItem &sli) {
  return std::visit(
      [&](auto &&arg1) -> LayoutRef2 {
        using T1 = std::decay_t<decltype(arg1)>;
        if constexpr (std::is_same_v<T1, StructType>) {
          return LayoutRef2{.lt = lt, .ref = lt.structLookup.at(arg1)};
        } else if constexpr (std::is_same_v<T1, ArrayType>) {
          return synthesizeArrayInnerRef(sli, *this, arg1, arg1->elementType);
        } else {
          throw std::logic_error("Type does not have an inner type");
        }
      },
      sli.type);
}

} // namespace gfx::shader

#endif /* C9B5D141_DE0C_4173_97E1_0E5367526CFA */
