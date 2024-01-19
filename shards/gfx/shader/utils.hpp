#ifndef D81E0BF8_F71D_472E_958F_062D9D1FE4AE
#define D81E0BF8_F71D_472E_958F_062D9D1FE4AE

#include "fwd.hpp"
#include <cctype>

namespace gfx::shader {
inline String sanitizeIdentifier(StringView inName) {
  String result;
  for (auto c : inName) {
    if (std::isalnum(c) || c == '_') {
      result += c;
    } else {
      result += '_';
    }
  }
  return result;
}
} // namespace gfx::shader

#endif /* D81E0BF8_F71D_472E_958F_062D9D1FE4AE */
