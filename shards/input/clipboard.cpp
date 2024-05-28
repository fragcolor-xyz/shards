#include "clipboard.hpp"
#include "../core/platform.hpp"
#if SH_EMSCRIPTEN
#include "emscripten_browser_clipboard.h"
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
  struct PasteHandler {
    std::atomic_bool ready;
    std::string *buffer = new std::string();
  } handler;
  emscripten_browser_clipboard::paste(
      [](std::string const &data, void *ud) {
        PasteHandler *ph = (PasteHandler *)ud;
        *ph->buffer = data;
        ph->ready = true;
      },
      &handler);
  while (!handler.ready) {
    emscripten_sleep(1);
  }
  return Clipboard(handler.buffer);
}
void setClipboard(const char *data) { emscripten_browser_clipboard::copy(data); }
Clipboard::~Clipboard() {
  if (data) {
    delete (std::string *)data;
  }
}
Clipboard::operator std::string_view() const { return *(std::string *)data; }
#endif

} // namespace shards::input
