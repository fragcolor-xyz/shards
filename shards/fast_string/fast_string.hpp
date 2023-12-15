#ifndef EC76A763_D9F1_4187_B602_A2E23F686D7F
#define EC76A763_D9F1_4187_B602_A2E23F686D7F

#include "storage.hpp"
#include <string_view>
#include <string>
#include <functional>

namespace shards::fast_string {
struct FastString {
  uint64_t id = size_t(~0);

  FastString() = default;
  FastString(const std::string &str) { initFrom(std::string_view(str)); }
  FastString(std::string_view str) { initFrom(str); }
  FastString(const char *cstr) { initFrom(std::string_view(cstr)); }
  ~FastString() = default;

  std::strong_ordering operator<=>(const FastString &other) const = default;
  bool operator==(const FastString &other) const = default;
  bool operator<(const FastString &other) const = default;

  std::string_view str() const { return getData(); }
  const char *c_str() const {
    if (empty())
      return "";
    // Data is stored null terminated
    return getData().data();
  }

  FastString &operator=(const std::string& s) {
    initFrom(s);
    return *this;
  }
  FastString &operator=(std::string_view sv) {
    initFrom(sv);
    return *this;
  }
  FastString &operator=(const char *cstr) {
    initFrom(std::string_view(cstr));
    return *this;
  }

  void clear() { id = size_t(~0); }
  size_t size() const { return str().size(); }
  size_t empty() const { return id == size_t(~0); }

  explicit operator std::string() const { return std::string(str()); }
  operator std::string_view() const { return str(); }

private:
  void initFrom(std::string_view sv) {
    if (sv.empty()) {
      id = size_t(~0);
      return;
    }
    id = store(sv);
  }
  std::string_view getData() const {
    if (empty()) {
      return {};
    }
    return load(id);
  }
};
} // namespace shards::fast_string

template <> struct std::hash<shards::fast_string::FastString> {
  size_t operator()(const shards::fast_string::FastString &str) const { return std::hash<uint64_t>()(str.id); }
};

#endif /* EC76A763_D9F1_4187_B602_A2E23F686D7F */
