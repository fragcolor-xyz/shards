#ifndef FA80AE5A_453D_41E6_BA3D_78E6B3AA8D9C
#define FA80AE5A_453D_41E6_BA3D_78E6B3AA8D9C

#include <boost/container/stable_vector.hpp>

namespace shards {

template <typename T> struct ManyContainer {
private:
  boost::container::stable_vector<T> data;
  size_t allocIndex{};
  size_t version = size_t(-1);

public:
  template <typename F> T &pull(size_t version_, F &&makeNew = []() { return T(); }) {
    if (version != version_) {
      reset(version_);
    }
    if (allocIndex >= data.size()) {
      data.push_back(makeNew());
    }
    return data[allocIndex++];
  }

  void clear() {
    data.clear();
    allocIndex = 0;
  }

private:
  void reset(size_t version_) {
    allocIndex = 0;
    version = version_;
  }
};
} // namespace shards

#endif /* FA80AE5A_453D_41E6_BA3D_78E6B3AA8D9C */
