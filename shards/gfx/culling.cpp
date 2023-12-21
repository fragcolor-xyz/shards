#include "bounds.hpp"
#include <boost/container/flat_map.hpp>

namespace gfx {
struct CullingSet {
  void insertNode();
  void removeNode();
};

} // namespace gfx