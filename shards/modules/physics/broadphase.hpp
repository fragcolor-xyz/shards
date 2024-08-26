#ifndef FCBCB2CD_0F6E_4BCA_8066_D64C391FE177
#define FCBCB2CD_0F6E_4BCA_8066_D64C391FE177

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace shards::Physics {
namespace BroadPhaseLayers {
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
}; // namespace BroadPhaseLayers

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
  BPLayerInterface() {}

  virtual uint32_t GetNumBroadPhaseLayers() const override { return std::size(layers); }

  virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
    JPH_ASSERT(inLayer < std::size(layers));
    return layers[inLayer];
  }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
    switch ((JPH::BroadPhaseLayer::Type)inLayer) {
    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
      return "NON_MOVING";
    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
      return "MOVING";
    default:
      JPH_ASSERT(false);
      return "INVALID";
    }
  }
#endif

private:
  static inline JPH::BroadPhaseLayer layers[] = {BroadPhaseLayers::NON_MOVING, BroadPhaseLayers::MOVING};
};
} // namespace shards::Physics

#endif /* FCBCB2CD_0F6E_4BCA_8066_D64C391FE177 */
