#ifndef FE26371A_8A6F_4FF7_AE99_1B5051F5A5FB
#define FE26371A_8A6F_4FF7_AE99_1B5051F5A5FB

#include "../fwd.hpp"
#include <boost/container/small_vector.hpp>
#include <boost/core/span.hpp>

namespace gfx::anim {

static const char internalComponentIdentifier = '$';

// Checks if the given name points to an internal glTF component (translation/rotation/scale)
static inline bool isGltfBuiltinTarget(FastString path) { 
  static FastString $t = "$t";
  static FastString $r = "$r";
  static FastString $s = "$s";
  return path == $t || path == $r || path == $s;
}

struct Path {
  boost::container::small_vector<FastString, 12> path;

  Path() = default;
  Path(boost::span<const FastString> components) : path(components.begin(), components.end()) {}

  void push_back(FastString str) { path.push_back(str); }
  void clear() { path.clear(); }

  Path next(size_t skip = 1) const {
    if (skip > path.size())
      return Path();
    auto span = boost::span(path);
    return Path(span.subspan(skip));
  }

  operator bool() const { return path.size() > 0; }

  // Gets the head path component
  FastString getHead() const {
    shassert(path.size() > 0);
    return path.front();
  }
};
} // namespace gfx::anim

#endif /* FE26371A_8A6F_4FF7_AE99_1B5051F5A5FB */
