#ifndef CB21101C_6D49_4BDB_B871_F40DBCC60DF3
#define CB21101C_6D49_4BDB_B871_F40DBCC60DF3

#include <shards/core/foundation.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <shards/linalg_shim.hpp>
#include <gfx/linalg.hpp>

namespace gfx {

struct TargetCameraState {
  float3 pivot{};
  float distance{};
  float2 rotation{};

  static TargetCameraState fromLookAt(float3 pos, float3 target) {
    float distance = linalg::length(target - pos);

    float3 dir = (target - pos) / distance;
    // float zl = std::sqrt(dir.z * dir.z + dir.x * dir.x);
    float yaw = std::atan2(-dir.x, -dir.z);
    float pitch = std::asin(dir.y);

    TargetCameraState result;
    result.rotation = float2(pitch, yaw);
    result.pivot = target;
    result.distance = distance;
    return result;
  }

  static TargetCameraState fromTable(shards::TableVar &inputTable) {
    return TargetCameraState{
        .pivot = toFloat3(inputTable.get<shards::Var>("pivot")),
        .distance = (float)(inputTable.get<shards::Var>("distance")),
        .rotation = (float2)toFloat2(inputTable.get<shards::Var>("rotation")),
    };
  }
};

struct TargetCameraStateTable : public shards::TableVar {
  static inline std::array<SHVar, 3> _keys{
      shards::Var("pivot"),
      shards::Var("distance"),
      shards::Var("rotation"),
  };
  static inline shards::Types _types{{
      shards::CoreInfo::Float3Type,
      shards::CoreInfo::FloatType,
      shards::CoreInfo::Float2Type,
  }};
  static inline shards::Type Type = shards::Type::TableOf(_types, _keys);

  TargetCameraStateTable()
      : shards::TableVar(),                     //
        pivot(get<shards::Var>("pivot")),       //
        distance(get<shards::Var>("distance")), //
        rotation(get<shards::Var>("rotation")) {}

  const TargetCameraStateTable &operator=(const TargetCameraState &state) {
    distance = shards::Var(state.distance);
    pivot = shards::toVar(state.pivot);
    rotation = shards::toVar(state.rotation);
    return *this;
  }

  shards::Var &pivot;
  shards::Var &distance;
  shards::Var &rotation;
};
} // namespace gfx

#endif /* CB21101C_6D49_4BDB_B871_F40DBCC60DF3 */
