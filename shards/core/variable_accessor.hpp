#ifndef B8194505_DBA4_4E17_9B26_701B2563B719
#define B8194505_DBA4_4E17_9B26_701B2563B719

#include <shards/core/pmr/vector.hpp>
#include <compare>

namespace shards {
namespace compose {
struct VA_Variable {
  // The unique variable identifier/index for this compose session
  size_t index;
  // Debugger readable actual name of the variable
  std::string_view debugName;
  bool operator==(const VA_Variable &other) const { return index == other.index; }
  bool operator<(const VA_Variable &other) const { return index < other.index; }
};

struct VA_SubPath {
  OwnedVar key;
  bool operator==(const VA_SubPath &other) const { return key == other.key; }
  bool operator<(const VA_SubPath &other) const { return key < other.key; }
};

struct VA_Input {
  size_t scopeId;
  bool operator==(const VA_Input &other) const { return scopeId == other.scopeId; }
  bool operator<(const VA_Input &other) const { return scopeId < other.scopeId; }
};

struct VA_WireOutput {
  size_t wireId;
  // Debugger readable actual name of the wire
  std::string_view debugName;
  bool operator==(const VA_WireOutput &other) const { return wireId == other.wireId; }
  bool operator<(const VA_WireOutput &other) const { return wireId < other.wireId; }
};

struct VariableAccessor {
  std::variant<VA_Variable, VA_SubPath, VA_Input, VA_WireOutput> value;

  static VariableAccessor var(size_t index, std::string_view debugName = std::string_view{}) {
    return VariableAccessor{VA_Variable{index, debugName}};
  }
  static VariableAccessor wireOutput(size_t wireId, std::string_view debugName = std::string_view{}) {
    return VariableAccessor{VA_WireOutput{wireId, debugName}};
  }
  static VariableAccessor key(const SHVar &key) { return VariableAccessor{VA_SubPath{key}}; }
  static VariableAccessor input(size_t scopeId) { return VariableAccessor{VA_Input{scopeId}}; }

  template <typename T> T &as() { return std::get<T>(value); }
  template <typename T> const T &as() const { return std::get<T>(value); }
  template <typename T> bool is() const { return std::holds_alternative<T>(value); }

  bool operator==(const VariableAccessor &other) const { return value == other.value; }
  bool operator<(const VariableAccessor &other) const { return value < other.value; }
};

struct VariableAccessorChain {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;
  shards::pmr::vector<VariableAccessor> path;

  VariableAccessorChain(std::allocator_arg_t, allocator_type mr) : path(mr) { path.reserve(4); }
  VariableAccessorChain(std::allocator_arg_t, allocator_type mr, const VariableAccessor &root) : path(mr) {
    path.reserve(4);
    path.push_back(root);
  }
  VariableAccessorChain(std::allocator_arg_t, allocator_type mr, const VariableAccessorChain &other) : path(mr) {
    path.reserve(other.path.size());
    path.insert(path.end(), other.path.begin(), other.path.end());
  }
  VariableAccessorChain(allocator_type mr, const VariableAccessor &root) : VariableAccessorChain(std::allocator_arg, mr, root) {}
  VariableAccessorChain(std::allocator_arg_t, allocator_type mr, VariableAccessorChain &&other)
      : path(std::move(other.path), mr) {}

  void append(VariableAccessor va) { path.push_back(va); }

  bool operator==(const VariableAccessorChain &other) const {
    return std::equal(path.begin(), path.end(), other.path.begin(), other.path.end());
  }
  bool operator<(const VariableAccessorChain &other) const {
    return std::lexicographical_compare(path.begin(), path.end(), other.path.begin(), other.path.end());
  }
};

SHTypeInfo *resolveVariableSubPath(const SHTypeInfo &type, const VA_SubPath &subPath);

} // namespace compose
} // namespace shards

namespace std {

template <> struct hash<shards::compose::VA_Variable> {
  size_t operator()(const shards::compose::VA_Variable &va) const { return std::hash<size_t>{}(va.index); }
};

template <> struct hash<shards::compose::VA_SubPath> {
  size_t operator()(const shards::compose::VA_SubPath &va) const { return std::hash<shards::OwnedVar>{}(va.key); }
};

template <> struct hash<shards::compose::VA_Input> {
  size_t operator()(const shards::compose::VA_Input &va) const { return std::hash<size_t>{}(va.scopeId); }
};

template <> struct hash<shards::compose::VA_WireOutput> {
  size_t operator()(const shards::compose::VA_WireOutput &va) const { return std::hash<size_t>{}(va.wireId); }
};

template <> struct hash<shards::compose::VariableAccessor> {
  size_t operator()(const shards::compose::VariableAccessor &va) const { return std::hash<decltype(va.value)>{}(va.value); }
};

template <> struct hash<shards::compose::VariableAccessorChain> {
  size_t operator()(const shards::compose::VariableAccessorChain &chain) const {
    size_t seed = 0;
    for (const auto &va : chain.path) {
      seed ^= std::hash<shards::compose::VariableAccessor>{}(va) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};
} // namespace std

#endif /* B8194505_DBA4_4E17_9B26_701B2563B719 */
