#include "tracking.hpp"
#include "shards.h"
#include "spdlog/spdlog.h"
#include <client/TracyCallstack.hpp>
#include <common/TracyAlloc.hpp>
#include <mutex>
#include <stdexcept>
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

template <typename T> struct VirtualAllocationName {
  static const std::string name;
  static const char *get() { return name.c_str(); }
};
template <typename T> const std::string VirtualAllocationName<T>::name = std::string(NAMEOF_SHORT_TYPE(T)) + " count";

static bool shuttingDown{};

struct Tracker {
  struct TrackedObject {};
  MultiRegistry registries;

  struct TrackedArray {
    const TypeInfo *ti;
    uint32_t length;
  };
  std::unordered_map<void *, TrackedArray> arraysRegistry;

  std::mutex arraysLock;
  std::mutex registryLock;

  std::mutex allocatedLock;
  std::unordered_map<void *, void *> allocated;

  static constexpr int CallStackDepth = 16;

  Tracker() {
    gfxTracyInit();
    atexit([]() { shuttingDown = true; });
  }

#if defined(SHARDS_TRACK_MATCHING_FREE_ALLOC) && defined(TRACY_HAS_CALLSTACK)
  void dumpCallstack(void *callstack) {
    auto data = (uintptr_t *)callstack;
    const auto sz = *data++;
    uintptr_t i;
    for (i = 0; i < sz; i++) {
      const char *v = tracy::DecodeCallstackPtrFast(data[i]);
      SPDLOG_CRITICAL(" {}", v);
    }
  }

  void debugCheckAlloc(void *ptr) {
    allocatedLock.lock();
    auto it = allocated.find(ptr);
    if (it != allocated.end()) {
      dumpCallstack(it->second);
      throw std::logic_error("");
    }
    allocated.emplace(ptr, tracy::Callstack(32));
    allocatedLock.unlock();
  }

  void debugCheckDealloc(void *ptr) {
    allocatedLock.lock();
    auto it = allocated.find(ptr);
    if (it == allocated.end()) {
      dumpCallstack(it->second);
      throw std::logic_error("");
    }
    allocated.erase(it);
    allocatedLock.unlock();
  }
#else
  ALWAYS_INLINE void debugCheckAlloc(void *) {}
  ALWAYS_INLINE void debugCheckDealloc(void *) {}
#endif

  template <typename T> void track(T *item) {
    debugCheckAlloc(item);

    registryLock.lock();
    auto &reg = std::get<Registry<T>>(registries);
    reg.emplace(item, uint8_t{});
    registryLock.unlock();

    tracy::Profiler::MemAllocCallstackNamed(item, 1, CallStackDepth, false, VirtualAllocationName<T>::get());
  }

  template <typename T> void untrack(T *item) {
    debugCheckDealloc(item);

    registryLock.lock();
    auto &reg = std::get<Registry<T>>(registries);
    auto it = reg.find(item);
    if (it != reg.end()) {
      reg.erase(it);
      tracy::Profiler::MemFreeCallstackNamed(item, CallStackDepth, false, VirtualAllocationName<T>::get());
    }
    registryLock.unlock();
  }

  void trackArray(const TypeInfo &ti, void *data, size_t length) {
    debugCheckAlloc(data);

    arraysLock.lock();
    auto &entry = arraysRegistry[data];
    arraysLock.unlock();
    entry.ti = &ti;
    entry.length = length;
    tracy::Profiler::MemAllocCallstackNamed(data, length * ti.size, CallStackDepth, false, ti.name.c_str());
  }

  void untrackArray(const TypeInfo &ti, void *data) {
    debugCheckDealloc(data);

    arraysLock.lock();
    auto it = arraysRegistry.find(data);
    if (it != arraysRegistry.end()) {
      arraysRegistry.erase(it);
      tracy::Profiler::MemFreeCallstackNamed(data, CallStackDepth, false, ti.name.c_str());
    }
    arraysLock.unlock();
  }

  static Tracker &get() {
    static Tracker inst;
    return inst;
  }
};

#define SHARDS_IMPL_TRACKING(__type) \
  void track(__type *v) {            \
    if (!shuttingDown)               \
      Tracker::get().track(v);       \
  }                                  \
  void untrack(__type *v) {          \
    if (!shuttingDown)               \
      Tracker::get().untrack(v);     \
  }
SHARDS_IMPL_TRACKING(SHSetImpl)
SHARDS_IMPL_TRACKING(SHTableImpl)
SHARDS_IMPL_TRACKING(SHWire)
void trackArray(const TypeInfo &ti, void *data, size_t length) {
  if (!shuttingDown)
    Tracker::get().trackArray(ti, data, length);
}
void untrackArray(const TypeInfo &ti, void *data) {
  if (!shuttingDown)
    Tracker::get().untrackArray(ti, data);
}
} // namespace shards::tracking