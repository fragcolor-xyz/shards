#ifndef CFB9369D_F72D_4EA0_BD57_F57DF65999C2
#define CFB9369D_F72D_4EA0_BD57_F57DF65999C2

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

namespace shards {
struct Cached {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Computes a value"); }

  PARAM(ShardsVar, _evaluate, "Evaluate", "The shards to evaluate the cached value based on input", {shards::CoreInfo::Shards});
  PARAM_IMPL(PARAM_IMPL_FOR(_evaluate));

  OwnedVar _lastInput;
  SHVar _lastOutput{};

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHTypeInfo compose(SHInstanceData &data) {
    SHComposeResult res = _evaluate.compose(data);
    if (res.failed)
      throw std::runtime_error("Failed to compose Cached evaluate expression");
    return res.outputType;
  }

  SHExposedTypesInfo requiredVariables() { return _evaluate.composeResult().requiredInfo; }
  SHExposedTypesInfo exposedVariables() { return _evaluate.composeResult().exposedInfo; }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (_lastInput != input) {
      _lastInput = input;
      // Need to try-catch because inlined shard doesn't handle
      try {
        _evaluate.activate(shContext, input, _lastOutput);
      } catch (std::exception &e) {
        shards::abortWire(shContext, e.what());
      }
    }
    return _lastOutput;
  }
};
} // namespace shards

#endif /* CFB9369D_F72D_4EA0_BD57_F57DF65999C2 */
