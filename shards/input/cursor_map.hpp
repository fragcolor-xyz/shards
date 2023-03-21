#ifndef E7523CEA_AA88_4F7F_8304_1C349E788774
#define E7523CEA_AA88_4F7F_8304_1C349E788774

#include <SDL_mouse.h>
#include <unordered_map>

namespace shards::input {
struct SDLCursor {
  SDL_Cursor *cursor{};
  SDLCursor(SDL_SystemCursor id) { cursor = SDL_CreateSystemCursor(id); }
  SDLCursor(SDLCursor &&rhs) {
    cursor = rhs.cursor;
    rhs.cursor = nullptr;
  }
  SDLCursor(const SDLCursor &) = delete;
  SDLCursor &operator=(const SDLCursor &) = delete;
  SDLCursor &operator=(SDLCursor &&rhs) {
    cursor = rhs.cursor;
    rhs.cursor = nullptr;
    return *this;
  }
  ~SDLCursor() {
    if (cursor)
      SDL_FreeCursor(cursor);
  }
  operator SDL_Cursor *() const { return cursor; }
};

struct CursorMap {
  std::unordered_map<SDL_SystemCursor, SDLCursor> cursorMap{};

  CursorMap() {
    cursorMap.insert_or_assign(SDL_SYSTEM_CURSOR_IBEAM, SDLCursor(SDL_SYSTEM_CURSOR_IBEAM));
    cursorMap.insert_or_assign(SDL_SYSTEM_CURSOR_HAND, SDLCursor(SDL_SYSTEM_CURSOR_HAND));
    cursorMap.insert_or_assign(SDL_SYSTEM_CURSOR_CROSSHAIR, SDLCursor(SDL_SYSTEM_CURSOR_CROSSHAIR));
    cursorMap.insert_or_assign(SDL_SYSTEM_CURSOR_SIZENESW, SDLCursor(SDL_SYSTEM_CURSOR_SIZENESW));
    cursorMap.insert_or_assign(SDL_SYSTEM_CURSOR_SIZENWSE, SDLCursor(SDL_SYSTEM_CURSOR_SIZENWSE));
    cursorMap.insert_or_assign(SDL_SYSTEM_CURSOR_ARROW, SDLCursor(SDL_SYSTEM_CURSOR_ARROW));
    cursorMap.insert_or_assign(SDL_SYSTEM_CURSOR_SIZENS, SDLCursor(SDL_SYSTEM_CURSOR_SIZENS));
    cursorMap.insert_or_assign(SDL_SYSTEM_CURSOR_SIZEWE, SDLCursor(SDL_SYSTEM_CURSOR_SIZEWE));
  }

  SDL_Cursor *getCursor(SDL_SystemCursor cursor) {
    auto it = cursorMap.find(cursor);
    if (it == cursorMap.end())
      return nullptr;
    return it->second;
  }

  static inline CursorMap &getInstance() {
    static CursorMap map;
    return map;
  }
};
} // namespace shards::input

#endif /* E7523CEA_AA88_4F7F_8304_1C349E788774 */
