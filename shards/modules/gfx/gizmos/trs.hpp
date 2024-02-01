#ifndef DE020B53_8639_4F3C_B987_87E433D5FF2E
#define DE020B53_8639_4F3C_B987_87E433D5FF2E

#include <gfx/trs.hpp>
#include <gfx/linalg.hpp>
#include <shards/shards.hpp>
#include <shards/linalg_shim.hpp>

namespace gfx {
struct TRSVars {
  static inline shards::Var v_scale{"scale"};
  static inline shards::Var v_translation{"translation"};
  static inline shards::Var v_rotation{"rotation"};
};

inline TRSRef toTrsRef(const SHVar &input_) {
  shards::TableVar &input = (shards::TableVar &)input_;
  float3 &scale = (float3 &)input[TRSVars::v_scale].payload;
  float3 &translation = (float3 &)input[TRSVars::v_translation].payload;
  float4 &rotation = (float4 &)input[TRSVars::v_rotation].payload;
  return TRSRef(translation, rotation, scale);
}

inline void toVar(shards::TableVar &tv, const TRSRef &trs) {
  auto &vt = tv[TRSVars::v_translation];
  auto &vs = tv[TRSVars::v_scale];
  auto &vr = tv[TRSVars::v_rotation];
  vt.valueType = SHType::Float3;
  vs.valueType = SHType::Float3;
  vr.valueType = SHType::Float4;
  (float3 &)vs.payload = trs.scale;
  (float3 &)vt.payload = trs.translation;
  (float4 &)vr.payload = trs.rotation;
}
} // namespace gfx

#endif /* DE020B53_8639_4F3C_B987_87E433D5FF2E */
