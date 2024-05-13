#include "clipboard.hpp"
#include "../core/platform.hpp"
#if SH_EMSCRIPTEN
#else
#include <SDL_clipboard.h>
#endif

namespace shards::input {
Clipboard::Clipboard(void *impl) : data(impl) {}
Clipboard::~Clipboard() {
  if (data) {
#if !SH_EMSCRIPTEN
    SDL_free(data);
#endif
  }
}
Clipboard::Clipboard(Clipboard &&other) {
  data = other.data;
  other.data = nullptr;
}
Clipboard &Clipboard::operator=(Clipboard &&) { return *this; }
Clipboard::operator std::string_view() const { return (const char *)data; }

} // namespace shards::input
