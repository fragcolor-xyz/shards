#ifndef FCAC19B2_68DE_4EFB_9A16_2D5504D727D8
#define FCAC19B2_68DE_4EFB_9A16_2D5504D727D8

#include "shards.h"
#include "nameof.hpp"
#include <string>

// Profiler functions for tracking allocations

struct SHSetImpl;
struct SHTableImpl;
namespace shards::tracking {
struct TypeInfo {
  template <typename T> static inline TypeInfo &get() {
    static TypeInfo inst{
        .name = std::string(NAMEOF_FULL_TYPE(T)),
        .size = sizeof(T),
    };
    return inst;
  }
  std::string name;
  size_t size;
};

#ifdef SHARDS_TRACKING
#define SHARDS_IMPL_TRACKING(__type) \
  void track(__type *);              \
  void untrack(__type *);
#else
#define SHARDS_IMPL_TRACKING(__type) \
  inline void track(__type *) {}     \
  inline void untrack(__type *) {}

inline void trackArray(const TypeInfo &ti, void *data, size_t length) {}
inline void untrackArray(const TypeInfo &ti, void *data) {}
#endif

SHARDS_IMPL_TRACKING(SHSetImpl)
SHARDS_IMPL_TRACKING(SHTableImpl)
SHARDS_IMPL_TRACKING(SHWire)
void trackArray(const TypeInfo &ti, void *data, size_t length);
void untrackArray(const TypeInfo &ti, void *data);
template <typename T> void trackArray(T *data, size_t length) { trackArray(TypeInfo::get<T>(), data, length); }
template <typename T> void untrackArray(T *data) { untrackArray(TypeInfo::get<T>(), data); }
#undef SHARDS_IMPL_TRACKING
void plotDataPoints();
} // namespace shards::tracking

#endif /* FCAC19B2_68DE_4EFB_9A16_2D5504D727D8 */
