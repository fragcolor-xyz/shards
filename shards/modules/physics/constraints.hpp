#ifndef B7B7F553_2BE1_4BB9_8324_06DA3B054FB7
#define B7B7F553_2BE1_4BB9_8324_06DA3B054FB7

#include "physics.hpp"
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

namespace shards::Physics {

struct ConstraintParams {
  uint8_t constraintType = ~0;
  uint64_t bodyNodeIdA = ~0, bodyNodeIdB = ~0;

  virtual uint64_t updateParamHash0() = 0;
  virtual JPH::TwoBodyConstraintSettings *getSettings() = 0;
};

struct ConstraintNode {
  static std::atomic_uint64_t UidCounter;
  uint64_t uid = UidCounter++;

  std::shared_ptr<ConstraintParams> params;
  uint64_t paramHash0{};

  // Persist this node even if not touched during a frame
  bool persistence : 1 = false;
  // Can be used to toggle this node even if it has persistence
  bool enabled : 1 = true;

  template <typename T> const std::shared_ptr<T> getParamsAs() const { return std::static_pointer_cast<T>(params); }

  template <typename TParams> static std::shared_ptr<ConstraintNode> create() {
    auto node = std::make_shared<ConstraintNode>();
    node->params = std::make_shared<TParams>();
    return node;
  }
};
} // namespace shards::Physics

#endif /* B7B7F553_2BE1_4BB9_8324_06DA3B054FB7 */
