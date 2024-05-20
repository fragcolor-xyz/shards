#ifndef GFX_CONTEXT_DATA
#define GFX_CONTEXT_DATA

#include "../core/assert.hpp"
#include "unique_id.hpp"
#include <memory>
#include <mutex>
#include <concepts>
#include <boost/container/flat_map.hpp>

namespace gfx {
struct Context;

struct ContextData {
  size_t version = ~0;
  size_t lastChecked = ~0;
};

template <typename T>
concept TContextData = std::is_base_of_v<ContextData, T>;

template <typename T>
concept TWithContextData = requires(T t, typename T::ContextDataType &cd, Context &ctx) {
  typename T::ContextDataType;
  { t.initContextData(ctx, cd) };
  { t.updateContextData(ctx, cd) };
  { t.getVersion() } -> std::convertible_to<uint64_t>;
  { t.getId() } -> std::convertible_to<UniqueId>;
} && TContextData<typename T::ContextDataType>;

} // namespace gfx

#endif // GFX_CONTEXT_DATA
