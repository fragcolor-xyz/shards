#ifndef EAE30273_C65E_49F0_BEF0_69BE363EAF61
#define EAE30273_C65E_49F0_BEF0_69BE363EAF61

#include "foundation.hpp"
#include "shared.hpp"
#include <memory>

namespace shards {

enum RunWireMode { Inline, Detached, Stepped };
struct WireBase {
  typedef EnumInfo<RunWireMode> RunWireModeInfo;
  static inline RunWireModeInfo runWireModeInfo{"RunWireMode", CoreCC, 'runC'};
  static inline Type ModeType{{SHType::Enum, {.enumeration = {.vendorId = CoreCC, .typeId = 'runC'}}}};

  static inline Types WireTypes{{CoreInfo::WireType, CoreInfo::StringType, CoreInfo::NoneType}};

  static inline Types WireVarTypes{WireTypes, {CoreInfo::WireVarType}};

  static inline Parameters waitParamsInfo{
      {"Wire", SHCCSTR("The wire to wait."), {WireVarTypes}},
      {"Passthrough", SHCCSTR("The output of this shard will be its input."), {CoreInfo::BoolType}},
      {"Timeout",
       SHCCSTR("The optional amount of time in seconds to wait for the wire to complete, if the time elapses the wire will be "
               "stopped and the flow will fail with a timeout error."),
       {CoreInfo::FloatType, CoreInfo::FloatVarType, CoreInfo::NoneType}}};

  static inline Parameters stopWireParamsInfo{
      {"Wire", SHCCSTR("The wire to stop."), {WireVarTypes}},
      {"Passthrough", SHCCSTR("The output of this shard will be its input."), {CoreInfo::BoolType}}};

  static inline Parameters runWireParamsInfo{{"Wire", SHCCSTR("The wire to run."), {WireTypes}}};

  ParamVar wireref{};
  std::shared_ptr<SHWire> wire;
  bool passthrough{false};
  bool capturing{false};
  RunWireMode mode{RunWireMode::Inline};
  IterableExposedInfo exposedInfo{};

  void resetComposition();

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  std::unordered_set<const SHWire *> &gatheringWires();

  void verifyAlreadyComposed(const SHInstanceData &data, IterableExposedInfo &shared);

  SHTypeInfo compose(const SHInstanceData &data);

  void cleanup() { wireref.cleanup(); }

  void warmup(SHContext *ctx) { wireref.warmup(ctx); }

  // Use state to mark the dependency for serialization as well!

  SHVar getState() {
    if (wire) {
      return Var(wire);
    } else {
      SHLOG_TRACE("getState no wire was avail");
      return Var::Empty;
    }
  }

  void setState(SHVar state) {
    if (state.valueType == SHType::Wire) {
      wire = SHWire::sharedFromRef(state.payload.wireValue);
    }
  }
};
}

#endif /* EAE30273_C65E_49F0_BEF0_69BE363EAF61 */
