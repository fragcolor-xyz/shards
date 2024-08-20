#include "shards/core/module.hpp"
#include <shards/core/runtime.hpp>
#include <shards/common_types.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include <shards/input/clipboard.hpp>

namespace shards {
namespace UI::Clipboard {

struct SetClipboard {
  static SHOptionalString help() {
    return SHCCSTR("Sets the input string to the system clipboard.");
  }
  static SHOptionalString inputHelp() {
    return SHCCSTR("The string to set as the clipboard contents.");
  }
  static SHOptionalString outputHelp() {
    return DefaultHelpText::OutputHelpPass;
  }
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
  static SHOptionalString help() {
    return SHCCSTR("Retrieves the current system clipboard contents.");
  }
  static SHOptionalString inputHelp() {
    return DefaultHelpText::InputHelpIgnored;
  }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns the current clipboard contents as a string.");
  }
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