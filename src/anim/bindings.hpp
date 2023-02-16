#ifndef CA26CBD3_868D_4C65_B50F_82E471917DB3
#define CA26CBD3_868D_4C65_B50F_82E471917DB3

#include "shards.h"
#include <shards.hpp>

namespace shards::Animations {
struct Path {
  const SHVar *first{};
  size_t length{};

  Path() = default;
  Path(const SHVar *first, size_t length) : first(first), length(length) {}
  Path(const SHVar &path) {
    assert(path.valueType == SHType::Seq);
    first = path.payload.seqValue.elements;
    length = path.payload.seqValue.len;
  }

  Path next(size_t skip = 1) const {
    if (skip > length)
      return Path();
    return Path(first + skip, length - skip);
  }

  operator bool() const { return length > 0; }

  // Gets the head path component
  const char *getHead() const {
    assert(*this);
    assert(first->valueType == SHType::String);
    return first->payload.stringValue;
  }
};

struct AnimationBinder {
  void apply(Path path) {}
};

} // namespace shards::Animations
#endif /* CA26CBD3_868D_4C65_B50F_82E471917DB3 */
