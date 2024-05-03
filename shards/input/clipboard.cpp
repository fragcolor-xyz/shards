#include "clipboard.hpp"
#include "../core/platform.hpp"
#if SH_EMSCRIPTEN
#include <emscripten.h>
// #include "emscripten_browser_clipboard.h"
extern "C" {
void gfxClipboardGet(char **recv, std::atomic_bool *ready);
void gfxClipboardSet(char *data);
}
#else
#include <SDL_clipboard.h>
#endif

namespace shards::input {
Clipboard::Clipboard(void *impl) : data(impl) {}
Clipboard::Clipboard(Clipboard &&other) {
  data = other.data;
  other.data = nullptr;
}
Clipboard &Clipboard::operator=(Clipboard &&) { return *this; }

#if SHARDS_GFX_SDL
Clipboard getClipboard() { return Clipboard(SDL_GetClipboardText()); }
void setClipboard(const char *data) { SDL_SetClipboardText(data); }
Clipboard::~Clipboard() {
  if (data) {
    SDL_free(data);
  }
}
Clipboard::operator std::string_view() const { return (const char *)data; }
#else
Clipboard getClipboard() {
  char* recvPtr{};
  std::atomic_bool recvReady{};
  static_assert(sizeof(std::atomic_bool) == sizeof(bool), "Just checking");
  gfxClipboardGet(&recvPtr, &recvReady);
  while (!recvReady.load(std::memory_order_acquire)) {
    emscripten_sleep(1);
  }
  return Clipboard(recvPtr);
}
void setClipboard(const char *data) {
  char* dataCopy = strdup(data);
  gfxClipboardSet(dataCopy);
}
Clipboard::~Clipboard() {
  if (data) {
    free(data);
    // delete (std::string *)data;
  }
}
Clipboard::operator std::string_view() const { return (const char *)data; }
#endif

} // namespace shards::input
