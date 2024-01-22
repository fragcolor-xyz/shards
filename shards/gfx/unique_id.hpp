#ifndef F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE
#define F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE

#include <stdint.h>

#ifndef RUST_BINDGEN
#include <atomic>
#include <compare>
#endif

namespace gfx {

enum class UniqueIdTag : uint8_t {
  Mesh,
  Feature,
  Drawable,
  DrawQueue,
  Material,
  Step,
  Texture,
  View,
  Buffer
};

typedef uint64_t UniqueIdValue;

inline constexpr UniqueIdValue UniqueIdBits = 64;
inline constexpr UniqueIdValue UniqueIdTagBits = 8;
inline constexpr UniqueIdValue UniqueIdIdMask = (1llu << (UniqueIdBits - UniqueIdTagBits)) - 1llu;
inline constexpr UniqueIdValue UniqueIdTagMask = ~UniqueIdIdMask;

struct UniqueId;
struct UniqueId {
  UniqueIdValue value;

#ifndef RUST_BINDGEN
  constexpr UniqueId() = default;
  constexpr UniqueId(UniqueIdValue v) : value(v) {}
  constexpr operator UniqueIdValue() const { return value; }

  std::strong_ordering operator<=>(const UniqueId &) const = default;

  constexpr UniqueId withTag(UniqueIdTag tag) {
    UniqueIdValue tagPart = (UniqueIdValue(tag) << (UniqueIdBits - UniqueIdTagBits)) & UniqueIdTagMask;
    UniqueIdValue idPart = value & UniqueIdIdMask;
    return tagPart | idPart;
  }

  constexpr size_t getIdPart() { return value & UniqueIdIdMask; }

  constexpr UniqueIdTag getTag() {
    UniqueIdValue tagPart = value & UniqueIdTagMask;
    return UniqueIdTag(tagPart >> (UniqueIdBits - UniqueIdTagBits));
  }
#endif
};

#ifndef RUST_BINDGEN
// define: friend struct UpdateUniqueId<Type>;
template <typename T> struct UpdateUniqueId {
  void apply(T &elem, UniqueId newId) { elem.id = newId; }
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
    return UniqueId(counter.fetch_add(1, std::memory_order_relaxed)).withTag(tag);
  }

  // Test function to reset ID counters
  static void _resetAll();
  static void _register(std::weak_ptr<UniqueIdGenerator *> &&tracker);
};
} // namespace gfx

template <> struct std::hash<gfx::UniqueId> {
  size_t operator()(gfx::UniqueId v) const { return size_t(v.value); }
};
#endif

#endif /* F14E50FC_17BA_4A5F_BB8B_8CF2D95A9ECE */
