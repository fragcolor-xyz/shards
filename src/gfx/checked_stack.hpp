#ifndef A67E2D4D_B267_4EFB_9A3C_C2BF0D2FE420
#define A67E2D4D_B267_4EFB_9A3C_C2BF0D2FE420

#include <vector>
#include <stdexcept>
#include <spdlog/fmt/fmt.h>

namespace gfx {
template <typename T> struct CheckedStack : public std::vector<T> {
  typedef size_t Marker;
  using std::vector<T>::size;
  using std::vector<T>::resize;

public:
  Marker getMarker() const { return size(); }

  // Validates check balance compared to marker start
  // throws on unmatched push/pop
  // if caught stack will be restored to the passed marker
  void restoreMarkerChecked(Marker marker) {
    size_t numItemsBeforeFixup = size();

    // Fixup before throw in case error is handled
    resize(marker);

    if (numItemsBeforeFixup != marker) {
      throw std::runtime_error(fmt::format("Stack imbalance ({} elements, expected {})", numItemsBeforeFixup, marker));
    }
  }

  // Checks and throws if stack is not empty
  // if caught stack will be empty afterwards
  void reset() { restoreMarkerChecked(Marker(0)); }
};
} // namespace gfx

#endif /* A67E2D4D_B267_4EFB_9A3C_C2BF0D2FE420 */
