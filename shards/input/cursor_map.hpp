#ifndef E7523CEA_AA88_4F7F_8304_1C349E788774
#define E7523CEA_AA88_4F7F_8304_1C349E788774

#include <SDL3/SDL_mouse.h>
#include <unordered_map>
#include <magic_enum.hpp>

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
      SDL_DestroyCursor(cursor);
  }
  operator SDL_Cursor *() const { return cursor; }
};

struct CursorMap {
  std::unordered_map<SDL_SystemCursor, SDLCursor> cursorMap{};

  CursorMap() {
    magic_enum::enum_for_each<SDL_SystemCursor>([&](auto x) {
      if (x == SDL_NUM_SYSTEM_CURSORS)
        return;
      cursorMap.insert_or_assign(x, SDLCursor(x));
    });
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
