#ifndef ACE82164_D020_40B6_A9E1_A13621726A27
#define ACE82164_D020_40B6_A9E1_A13621726A27

#include <shards/core/object_var_util.hpp>
#include <shards/core/assert.hpp>
#include <shards/core/exposed_type_utils.hpp>
#include <shards/core/hasherxxh3.hpp>
#include <shards/core/async.hpp>
#include <shards/common_types.hpp>
#include <shards/log/log.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Math/MathTypes.h>
#include <Jolt/Math/Quat.h>
#include <boost/container/flat_map.hpp>
#include <boost/container/stable_vector.hpp>
#include <tracy/Wrapper.hpp>
#include <linalg.h>
#include <memory>
#include <atomic>

namespace shards {
inline SHVar toVar(const JPH::Float3 &f3) {
  SHVar v{};
  v.valueType = SHType::Float3;
  v.payload.float3Value[0] = f3.x;
  v.payload.float3Value[1] = f3.y;
  v.payload.float3Value[2] = f3.z;
  return v;
}
inline SHVar toVar(const JPH::Float4 &f4) {
  SHVar v{};
  v.valueType = SHType::Float4;
  v.payload.float4Value[0] = f4.x;
  v.payload.float4Value[1] = f4.y;
  v.payload.float4Value[2] = f4.z;
  v.payload.float4Value[3] = f4.w;
  return v;
}
inline SHVar toVar(const JPH::Vec3 &f3) {
  SHVar v{};
  v.valueType = SHType::Float3;
  v.payload.float3Value[0] = f3.GetX();
  v.payload.float3Value[1] = f3.GetY();
  v.payload.float3Value[2] = f3.GetZ();
  return v;
}
inline SHVar toVar(const JPH::Quat &f4) {
  SHVar v{};
  v.valueType = SHType::Float4;
  v.payload.float4Value[0] = f4.GetX();
  v.payload.float4Value[1] = f4.GetY();
  v.payload.float4Value[2] = f4.GetZ();
  v.payload.float4Value[3] = f4.GetW();
  return v;
}
} // namespace shards

namespace shards::Physics {
using namespace linalg::aliases;

inline shards::logging::Logger getLogger() { return shards::logging::getOrCreate("physics"); }

inline JPH::Float3 toJPHFloat3(const SHFloat3 &f3) { return JPH::Float3{f3[0], f3[1], f3[2]}; }
inline JPH::Float4 toJPHFloat4(const SHFloat4 &f4) { return JPH::Float4{f4[0], f4[1], f4[2], f4[3]}; }
inline JPH::Vec3 toJPHVec3(const SHFloat3 &f3) { return JPH::Vec3{f3[0], f3[1], f3[2]}; }
inline JPH::Vec4 toJPHVec4(const SHFloat4 &f4) { return JPH::Vec4{f4[0], f4[1], f4[2], f4[3]}; }
inline JPH::Quat toJPHQuat(const SHFloat4 &f4) { return JPH::Quat(f4[0], f4[1], f4[2], f4[3]); }

inline float3 toLinalg(const JPH::Float3 &f3) { return float3{f3.x, f3.y, f3.z}; }
inline float3 toLinalg(const JPH::Vec3 &f3) { return float3{f3.GetX(), f3.GetY(), f3.GetZ()}; }
inline float4 toLinalg(const JPH::Float4 &f4) { return float4{f4.x, f4.y, f4.z, f4.w}; }
inline float4 toLinalg(const JPH::Vec4 &f4) { return float4{f4.GetX(), f4.GetY(), f4.GetZ(), f4.GetW()}; }
inline float4 toLinalgLinearColor(const JPH::Color &f4) { return toLinalg(f4.ToVec4()); }

struct Node {
  static std::atomic_uint64_t UidCounter;
  uint64_t uid = UidCounter++;

  // Persist this node even if not touched during a frame
  bool persistence : 1 = false;
  // Can be used to toggle this node even if it has persistence
  bool enabled : 1 = true;

  // Counter that when > 0 indicates collision events need to be collected for this node
  uint16_t neededCollisionEvents{};

  // Transform parameters
  JPH::Vec3 location;
  JPH::Quat rotation;
  struct AssociatedData *data;

  OwnedVar tag;

  struct BodyParams {
    float friction;
    float resitution;
    float linearDamping;
    float angularDamping;
    float maxLinearVelocity;
    float maxAngularVelocity;
    float gravityFactor;
    JPH::EAllowedDOFs allowedDofs;
    JPH::EMotionType motionType;
    bool sensor;
    uint32_t groupMembership;
    uint32_t collisionMask;
  } params;

  JPH::Ref<JPH::Shape> shape;
  uint64_t shapeUid;

  uint64_t paramHash0;
  uint64_t paramHash1;

  void updateParamHash0();
  void updateParamHash1();
};

inline void Node::updateParamHash0() {
  shards::HasherXXH3<HasherDefaultVisitor, XXH64_hash_t> hasher;
  hasher(params.friction);
  hasher(params.resitution);
  hasher(params.linearDamping);
  hasher(params.angularDamping);
  paramHash0 = hasher.getDigest();
}

inline void Node::updateParamHash1() {
  shards::HasherXXH3<HasherDefaultVisitor, XXH64_hash_t> hasher;
  hasher(params.maxLinearVelocity);
  hasher(params.maxAngularVelocity);
  hasher(params.gravityFactor);
  hasher((uint8_t &)params.allowedDofs);
  hasher((uint8_t &)params.motionType);
  paramHash1 = hasher.getDigest();
}

struct Core;
struct SHBody {
  static inline int32_t ObjectId = 'phNo';
  static inline const char VariableName[] = "Physics.Body";
  static inline shards::Type Type = Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline shards::Type VarType = Type::VariableOf(Type);

  static inline shards::ObjectVar<SHBody, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                     RawType.object.typeId};
  std::shared_ptr<Core> core;
  std::shared_ptr<Node> node;
};

struct SHShape {
  static inline int32_t ObjectId = 'phSh';
  static inline const char VariableName[] = "Physics.Shape";
  static inline shards::Type Type = Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline shards::Type VarType = Type::VariableOf(Type);

  static inline shards::ObjectVar<SHShape, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                      RawType.object.typeId};

  static std::atomic_uint64_t UidCounter;
  uint64_t uid = UidCounter++;
  JPH::Ref<JPH::Shape> shape;
};

} // namespace shards::Physics

#endif /* ACE82164_D020_40B6_A9E1_A13621726A27 */
