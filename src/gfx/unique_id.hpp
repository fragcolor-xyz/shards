#ifndef F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE
#define F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE

#include <atomic>

namespace gfx {

using UniqueId = uint64_t;
struct UniqueIdGenerator {
private:
  std::atomic<UniqueId> counter;

public:
  UniqueId getNext() {
    return counter.fetch_add(1, std::memory_order_relaxed);
  }
};
} // namespace gfx

#endif /* F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE */
