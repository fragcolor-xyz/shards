#ifndef CD7D9B52_EC6C_4F1D_B3BA_94E6659C7FA7
#define CD7D9B52_EC6C_4F1D_B3BA_94E6659C7FA7

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

namespace shards {
struct Isolate {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }

  static SHOptionalString help() { return SHCCSTR("Isolates the inner shards' environment by only allowing certain variables"); }

  PARAM(ShardsVar, _contents, "Contents", "", {CoreInfo::Shards});
  PARAM_VAR(_include, "Include", "Includes only the listed variables", {CoreInfo::NoneType, CoreInfo::StringSeqType});
  PARAM_VAR(_exclude, "Exclude", "Ignores all the listed variables", {CoreInfo::NoneType, CoreInfo::StringSeqType});
  PARAM_IMPL(PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_include), PARAM_IMPL_FOR(_exclude));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  // Use this directly, don't use PARAM_REQUIRED_VARIABLES()
  SHExposedTypesInfo requiredInfo() { return _contents.composeResult().requiredInfo; }

  SHTypeInfo compose(SHInstanceData &data) {
    ExposedInfo innerShared;
    for (auto &s : data.shared) {
      bool match{};

      if (!_include->isNone()) {
        for (auto &include : _include) {
          if (SHSTRVIEW(include) == s.name) {
            match = true;
            break;
          }
        }
      } else {
        match = true;
      }

      if (!_exclude->isNone()) {
        for (auto &exclude : _exclude) {
          if (SHSTRVIEW(exclude) == s.name) {
            match = false;
            break;
          }
        }
      }

      if (match) {
        SHLOG_DEBUG("Isolate: adding shared var: {}", s.name);
        innerShared.push_back(s);
      }
    }

    SHInstanceData tmpData = data;
    tmpData.privateContext = nullptr; // null this, in order to create a new context!
    tmpData.shared = SHExposedTypesInfo(innerShared);
    auto cr = _contents.compose(tmpData);

    SHLOG_DEBUG("== Isolated Reqs ==");
    for (auto &req : cr.requiredInfo) {
      SHLOG_DEBUG(" >{}", req.name);
    }

    return cr.outputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHVar out;
    if (_contents.activate(shContext, input, out) == SHWireState::Error)
      throw std::runtime_error("Isolate: inner shards activation failed");
    return out;
  }
};

SHARDS_REGISTER_FN(exposed) { REGISTER_SHARD("Isolate", Isolate); }
} // namespace shards

#endif /* CD7D9B52_EC6C_4F1D_B3BA_94E6659C7FA7 */
