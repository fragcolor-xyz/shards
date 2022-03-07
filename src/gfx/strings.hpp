#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace gfx {

inline std::string::const_iterator findCaseInsensitive(const std::string &str, const std::string &term) {
  return std::search(str.begin(), str.end(), term.begin(), term.end(),
                     [](char a, char b) { return std::toupper(a) == std::toupper(b); });
}

inline bool containsCaseInsensitive(const std::string &str, const std::string &term) {
  return findCaseInsensitive(str, term) != str.cend();
}

} // namespace gfx
