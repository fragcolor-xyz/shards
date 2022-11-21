#include "uuid.hpp"
#include <boost/uuid/random_generator.hpp>

namespace gfx {
UUID generateUUID() {
  static thread_local boost::uuids::random_generator generator;
  return generator();
}
} // namespace gfx