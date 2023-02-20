#include "tracking.hpp"
#include "shards.h"
#include <tracy/Tracy.hpp>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <variant>

// TODO: Move this to a more generic location
// Defined in the gfx rust crate
extern "C" void gfxTracyInit();

namespace shards::tracking {

template <typename T> struct Registry : public std::unordered_map<T *, uint8_t> {};
template <typename... TArgs> struct IntoRegistry_t : public std::tuple<Registry<std::remove_pointer_t<TArgs>>...> {};
template <template <typename...> typename T> struct WithTrackableTypes {
  using type = T<SHSetImpl *, SHTableImpl *, SHWire *, SHSeq *>;
};

using Item = WithTrackableTypes<std::variant>;
using MultiRegistry = WithTrackableTypes<IntoRegistry_t>::type;

struct Tracker {
  struct TrackedObject {};
  MultiRegistry registries;

  struct TrackedArray {
    const TypeInfo *ti;
    uint32_t length;
  };
  std::unordered_map<void *, TrackedArray> arraysRegistry;
  bool tracyInitialized{};

  static constexpr int CallStackDepth = 16;

  ALWAYS_INLINE void ensureTracyInit() {
    if (!tracyInitialized) {
      gfxTracyInit();
      tracyInitialized = true;
    }
  }

  Tracker() { ensureTracyInit(); }

  template <typename T> void track(T *item) { std::get<Registry<T>>(registries).emplace(item, uint8_t{}); }
  template <typename T> void untrack(T *item) { std::get<Registry<T>>(registries).erase(item); }
  void trackArray(const TypeInfo &ti, void *data, size_t length) {

    auto &entry = arraysRegistry[data];
    entry.ti = &ti;
    entry.length = length;
    tracy::Profiler::MemAllocCallstackNamed(data, length * ti.size, CallStackDepth, false, ti.name.c_str());
  }

  void untrackArray(const TypeInfo &ti, void *data) {
    arraysRegistry.erase(data);
    tracy::Profiler::MemFreeCallstackNamed(data, CallStackDepth, false, ti.name.c_str());
  }

  void plotDataPoints() {
    auto &tables = std::get<Registry<SHTableImpl>>(registries);
    auto &sets = std::get<Registry<SHSetImpl>>(registries);
    auto &wires = std::get<Registry<SHWire>>(registries);

    TracyPlot("SHTables", int64_t(tables.size()));
    TracyPlot("SHSets", int64_t(sets.size()));
    TracyPlot("SHWires", int64_t(wires.size()));
  }

  static Tracker &get() {
    static Tracker inst;
    return inst;
  }
};

#define SHARDS_IMPL_TRACKING(__type)                 \
  void track(__type *v) { Tracker::get().track(v); } \
  void untrack(__type *v) { Tracker::get().untrack(v); }
SHARDS_IMPL_TRACKING(SHSetImpl)
SHARDS_IMPL_TRACKING(SHTableImpl)
SHARDS_IMPL_TRACKING(SHWire)
void trackArray(const TypeInfo &ti, void *data, size_t length) { Tracker::get().trackArray(ti, data, length); }
void untrackArray(const TypeInfo &ti, void *data) { Tracker::get().untrackArray(ti, data); }
void plotDataPoints() { Tracker::get().plotDataPoints(); }
} // namespace shards::tracking