#ifndef BAF72001_29B7_424A_AD7D_70C476A48508
#define BAF72001_29B7_424A_AD7D_70C476A48508

#include "hasherxxh128.hpp"
#include "unique_id.hpp"
#include <unordered_map>
#include <set>

namespace gfx {
struct References {
  std::set<UniqueId> set;

  void add(UniqueId id) { set.insert(id); }
  template <typename T> void add(const std::shared_ptr<T> &ref) { add(ref->getId()); }

  bool contains(UniqueId id) const { return set.contains(id); }
  template <typename T> bool contains(const std::shared_ptr<T> &ref) const { return contains(ref->getId()); }

  void clear() { set.clear(); }
};

// Does 2 things while traversing gfx objects:
// - Collect the pipeline hash to determine pipeline permutation
// - Collect references to other gfx objects that the permutation depends on
struct PipelineHashCollector {
  References references;
  HasherXXH128<PipelineHashVisitor> hasher;

  void addReference(UniqueId id) { references.add(id); }
  template <typename T> void addReference(const std::shared_ptr<T> &ref) { references.add(ref->getId()); }
  template <typename T> void operator()(const T &val) { hasher(val); }

  void reset() {
    hasher.reset();
    references.clear();
  }
};
} // namespace gfx

#endif /* BAF72001_29B7_424A_AD7D_70C476A48508 */
