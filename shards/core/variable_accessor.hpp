#ifndef B8194505_DBA4_4E17_9B26_701B2563B719
#define B8194505_DBA4_4E17_9B26_701B2563B719

#include <spdlog/spdlog.h>
#include <spdlog/formatter.h>
#include <shards/core/pmr/vector.hpp>

namespace shards {
namespace flow {
struct VA_Ref {
  std::string_view name;
};

struct VA_SubPath {
  OwnedVar key;
};

struct VA_Input {
  // Marker for current input variable
};

struct VariableAccessor {
  std::variant<VA_Ref, VA_SubPath, VA_Input> value;

  static VariableAccessor var(std::string_view name) { return VariableAccessor{VA_Ref{name}}; }
  static VariableAccessor key(const SHVar &key) { return VariableAccessor{VA_SubPath{key}}; }
  static VariableAccessor input() { return VariableAccessor{VA_Input{}}; }

  template <typename T> T &as() { return std::get<T>(value); }
  template <typename T> const T &as() const { return std::get<T>(value); }
  template <typename T> bool is() const { return std::holds_alternative<T>(value); }
};

struct VariableAccessorChain {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;
  shards::pmr::vector<VariableAccessor> stack;

  VariableAccessorChain(std::allocator_arg_t, allocator_type mr) : stack(mr) {}
  VariableAccessorChain(std::allocator_arg_t, allocator_type mr, const VariableAccessor &root) : stack(mr) {
    stack.push_back(root);
  }
  void append(VariableAccessor va) { stack.push_back(va); }
};
} // namespace flow
} // namespace shards

namespace fmt {
// Formatter for VariableAccessor
template <> struct formatter<shards::flow::VariableAccessor> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }
  template <typename FormatContext> auto format(shards::flow::VariableAccessor const &va, FormatContext &ctx) {
    if (va.is<shards::flow::VA_Ref>()) {
      return format_to(ctx.out(), "{}", va.as<shards::flow::VA_Ref>().name);
    } else if (va.is<shards::flow::VA_SubPath>()) {
      return format_to(ctx.out(), "{}", va.as<shards::flow::VA_SubPath>().key);
    } else if (va.is<shards::flow::VA_Input>()) {
      return format_to(ctx.out(), "<input>");
    }
    return format_to(ctx.out(), "unknown");
  }
};

// Formatter for chain as a:b:c format
template <> struct formatter<shards::flow::VariableAccessorChain> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }
  template <typename FormatContext> auto format(shards::flow::VariableAccessorChain const &chain, FormatContext &ctx) {
    for (size_t i = 0; i < chain.stack.size(); i++) {
      if (i > 0) {
        format_to(ctx.out(), ":");
      }
      format_to(ctx.out(), "{}", chain.stack[i]);
    }
    return format_to(ctx.out(), "");
  }
};
} // namespace fmt
#endif /* B8194505_DBA4_4E17_9B26_701B2563B719 */
