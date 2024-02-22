#ifndef B5E982E3_F1E3_4610_BBF5_4C4FE1C098F3
#define B5E982E3_F1E3_4610_BBF5_4C4FE1C098F3

#include <vector>

namespace ui2 {
// Mechanism for instantiating multiple UI elements with a single shard
template <typename T, auto MakeNew> struct ManyUI {
private:
  std::vector<T> data;
  size_t allocIndex{};
  size_t version = size_t(-1);

public:
  T &pull(size_t version_) {
    if (version != version_) {
      reset(version_);
    }
    if (allocIndex >= data.size()) {
      data.push_back(MakeNew());
    }
    return data[allocIndex++];
  }

private:
  void reset(size_t version_) {
    allocIndex = 0;
    version = version_;
  }
};
} // namespace ui2

#endif /* B5E982E3_F1E3_4610_BBF5_4C4FE1C098F3 */
