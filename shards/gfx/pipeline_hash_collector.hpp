#ifndef BAF72001_29B7_424A_AD7D_70C476A48508
#define BAF72001_29B7_424A_AD7D_70C476A48508

#include "hasherxxh3.hpp"
#include "unique_id.hpp"
#include "linalg.hpp"
#include <unordered_map>
#include <set>

namespace gfx::detail {

// Specialize this for custom types
template <typename T, typename H, typename = void> struct PipelineHash {
};

template <typename T, typename H>
struct PipelineHash<T, H, std::void_t<decltype(std::declval<T>().getPipelineHash(*(H *)0), bool())>> {
  static void apply(const T &val, H &hasher) { val.getPipelineHash(hasher); }
};

template <typename H> struct PipelineHash<float4x4, H> {
  static void apply(const float4x4 &val, H &hasher) {
    hasher(val.x);
    hasher(val.y);
    hasher(val.z);
    hasher(val.w);
  }
};

template<typename T>
concept CanApplyPipelineHash = requires(const T &val, shards::DummyHasher hasher) {
  PipelineHash<T, shards::DummyHasher>::apply(val, hasher);
};

struct PipelineHashVisitor {
  template <CanApplyPipelineHash T, typename H> void operator()(const T &val, H &hasher) { PipelineHash<T, H>::apply(val, hasher); }
};

struct References {
  std::set<UniqueId> set;

  void add(UniqueId id) { set.insert(id); }
  template <typename T> void add(const std::shared_ptr<T> &ref) { add(ref->getId()); }

  bool contains(UniqueId id) const { return set.contains(id); }
  template <typename T> bool contains(const std::shared_ptr<T> &ref) const { return contains(ref->getId()); }

  void clear() { set.clear(); }
};

// Interface to store and retrieve hashes
struct IPipelineHashStorage {
  virtual std::optional<Hash128> getHash(UniqueId id) = 0;
  virtual void addHash(UniqueId id, Hash128 hash) = 0;
};

typedef HasherXXH3<PipelineHashVisitor> PipelineHasher;

// Does 2 things while traversing gfx objects:
// - Collect the pipeline hash to determine pipeline permutation
// - Whenever a reference is enountered to another shard object, hash that one too or used it's already computed hash
struct PipelineHashCollector {
  PipelineHasher hasher;

  // Used to store and retrieve hashes for already hashed objects
  IPipelineHashStorage *storage{};

  // Insert a reference to another shared object
  template <typename T> void operator()(const std::shared_ptr<T> &ref) {
    if (storage) {
      auto id = ref->getId();
      if (auto hash = storage->getHash(id)) {
        hasher(hash.value());
      } else {
        PipelineHashCollector childCollector{.storage = storage};
        childCollector.hashObject(ref);
        Hash128 computedHash = childCollector.hasher.getDigest();
        hasher(computedHash);

        // Store for future usage
        storage->addHash(id, computedHash);
      }
    } else {
      PipelineHashCollector childCollector;
      childCollector.hashObject(ref);
      Hash128 computedHash = childCollector.hasher.getDigest();
      hasher(computedHash);
    }
  }

  template <typename T> void hashObject(const std::shared_ptr<T> &ref) { hashObject(*ref.get()); }
  template <typename T> void hashObject(const T &ref) { ref.pipelineHashCollect(*this); }

  // Hash a regular value supported by PipelineHashVisitor
  template <typename T> void operator()(const T &val) { hasher(val); }

  Hash128 getDigest() const { return hasher.getDigest(); }

  void reset() { hasher.reset(); }

  std::optional<Hash128> getReferenceHash(UniqueId id);
  std::optional<Hash128> insertReferenceHash(UniqueId id, Hash128);
};
} // namespace gfx::detail

#endif /* BAF72001_29B7_424A_AD7D_70C476A48508 */
