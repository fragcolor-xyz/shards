#include "shards/core/module.hpp"
#include <shards/core/runtime.hpp>
#include <shards/common_types.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include <shards/input/clipboard.hpp>

namespace shards {
namespace UI::Clipboard {

struct SetClipboard {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  OwnedVar nullTerminatedCache;

  SHVar activate(SHContext *context, const SHVar &input) {
    nullTerminatedCache = input;      // clone will null terminate inputs don't guarantee null termination
    callOnMeshThread(context, [&]() { //
      input::setClipboard(nullTerminatedCache.payload.stringValue);
    });
    return input;
  }
};

struct GetClipboard {
  std::string _output;
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    auto clipboardPtr = input::getClipboard();
    _output = clipboardPtr;
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