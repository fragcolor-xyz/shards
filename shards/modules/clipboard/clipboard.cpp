#include "core/module.hpp"
#include <SDL.h>
#include <SDL_clipboard.h>
#include <shards/core/runtime.hpp>
#include <shards/common_types.hpp>

namespace shards {
namespace UI::Clipboard {

struct SetClipboard {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  OwnedVar nullTerminatedCache;

  SHVar activate(SHContext *context, const SHVar &input) {
    nullTerminatedCache = input; // clone will null terminate inputs don't guarantee null termination
    SDL_SetClipboardText(nullTerminatedCache.payload.stringValue);
    return input;
  }
};

struct GetClipboard {
  std::string _output;
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    auto clipboardPtr = SDL_GetClipboardText();
    _output = clipboardPtr;
    SDL_free(clipboardPtr);
    return Var(_output);
  }
};
} // namespace UI::Clipboard
} // namespace shards
SHARDS_REGISTER_FN(clipboard) {
  using namespace shards::UI::Clipboard;
  REGISTER_SHARD("UI.SetClipboard", SetClipboard);
  REGISTER_SHARD("UI.GetClipboard", GetClipboard);
}