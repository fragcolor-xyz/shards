#include <SDL.h>
#include <SDL_clipboard.h>
#include "runtime.hpp"
#include "common_types.hpp"

namespace shards::UI::Clipboard {

struct SetClipboard {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    SDL_SetClipboardText(input.payload.stringValue);
    return input;
  }
};

struct GetClipboard {
  std::string _output;
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    _output = SDL_GetClipboardText();
    return Var(_output);
  }
};

void registerShards() {
  REGISTER_SHARD("UI.SetClipboard", SetClipboard);
  REGISTER_SHARD("UI.GetClipboard", GetClipboard);
}
} // namespace shards::UI::Clipboard