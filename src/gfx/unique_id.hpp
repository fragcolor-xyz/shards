#ifndef F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE
#define F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE

#include <atomic>

namespace gfx {

enum class UniqueIdTag : uint8_t {
  Mesh,
  Feature,
  Drawable,
  Material,
  Step,
  Texture,
};

typedef uint64_t UniqueIdValue;
struct UniqueId {
  UniqueIdValue value;

  constexpr UniqueId() = default;
  constexpr UniqueId(UniqueIdValue v) : value(v) {}
  constexpr operator UniqueIdValue() const { return value; }
};

inline constexpr UniqueId UniqueIdBits = 64;
inline constexpr UniqueId UniqueIdTagBits = 8;
inline constexpr UniqueId UniqueIdIdMask = (1llu << (UniqueIdBits - UniqueIdTagBits)) - 1llu;
inline constexpr UniqueId UniqueIdTagMask = ~UniqueIdIdMask;

inline constexpr UniqueId withTag(UniqueId id, UniqueIdTag tag) {
  UniqueIdValue tagPart = (UniqueIdValue(tag) << (UniqueIdBits - UniqueIdTagBits)) & UniqueIdTagMask;
  UniqueIdValue idPart = id & UniqueIdIdMask;
  return tagPart | idPart;
}
inline constexpr UniqueIdTag getTag(UniqueId id) {
  UniqueIdValue tagPart = id & UniqueIdTagMask;
  return UniqueIdTag(tagPart >> (UniqueIdBits - UniqueIdTagBits));
}

// define: friend struct UpdateUniqueId<Type>;
template <typename T> struct UpdateUniqueId {
  void apply(T& elem, UniqueId newId) { elem.id = newId; }
};

template <typename T> std::shared_ptr<std::remove_cv_t<T>> cloneSelfWithId(T *_this, UniqueId newId) {
  auto result = std::make_shared<std::remove_cv_t<T>>(*_this);
  UpdateUniqueId<std::remove_cv_t<T>>{}.apply(*result.get(), newId);
  return result;
}

struct UniqueIdGenerator {
private:
  std::atomic<UniqueIdValue> counter;
  UniqueIdTag tag;

  std::shared_ptr<UniqueIdGenerator *> tracker;

public:
  UniqueIdGenerator(UniqueIdTag tag) : tag(tag) {
    tracker = std::make_shared<UniqueIdGenerator *>(this);
    _register(tracker);
  }
  UniqueIdGenerator(const UniqueIdGenerator &) = delete;
  UniqueIdGenerator(UniqueIdGenerator &&) = delete;

  UniqueId getNext() {
    // No ordering, just needs to be atomic
    // https://en.cppreference.com/w/cpp/atomic/memory_order#Relaxed_ordering
    return withTag(counter.fetch_add(1, std::memory_order_relaxed), tag);
  }

  // Test function to reset ID counters
  static void _resetAll();
  static void _register(std::weak_ptr<UniqueIdGenerator *> &&tracker);
};
} // namespace gfx

#endif /* F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE */
