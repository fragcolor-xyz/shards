
#include "physics.hpp"
#include "job_system.hpp"
#include "broadphase.hpp"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

namespace shards::Physics {
struct Core : public std::enable_shared_from_this<Core> {
  struct AssociatedData {
    std::shared_ptr<Node> node;
    size_t lastTouched{};

    JPH::Body *body{};
    bool bodyAdded{};
    uint64_t bodySettingsHash;

    // Iterator helper
    AssociatedData *next{};

    uint64_t paramHash0;
    uint64_t paramHash1;
    uint64_t shapeUid;
  };
  using MapType = boost::container::flat_map<Node *, AssociatedData, std::less<Node *>, //
                                             boost::container::stable_vector<std::pair<Node *, AssociatedData>>>;

private:
  static std::atomic_uint64_t UidCounter;
  uint64_t uid = UidCounter++;

  MapType nodeMap;
  std::vector<AssociatedData *> activeNodes;
  // std::vector<AssociatedData *> removedNodes;
  size_t frameCounter_{};

  JPH::PhysicsSystem physicsSystem;

  JPH::TempAllocatorImpl tempAllocator{1024 * 1024 * 4};
  TidePoolJobSystem jobSystem{};

  // Optimized iteration state
  // AssociatedData *firstNode{};
  // AssociatedData *iterator{};

  BPLayerInterface broadPhaseLayerInterface;
  JPH::ObjectVsBroadPhaseLayerFilter objBroadPhaseFilter;
  JPH::ObjectLayerPairFilter objLayerPairFilter;

public:
  void begin() {
    ++frameCounter_;
    activeNodes.clear();
    // removedNodes.clear();
  }

  uint64_t getId() const { return uid; }

  const MapType &getNodeMap() const { return nodeMap; }
  const auto &getActiveNodes() const { return activeNodes; }

  size_t frameCounter() const { return frameCounter_; }

  void init() { physicsSystem.Init(1024 * 8, 0, 1024, 1024, broadPhaseLayerInterface, objBroadPhaseFilter, objLayerPairFilter); }

  std::shared_ptr<Node> createNewNode() {
    auto node = std::make_shared<Node>();
    addNodeInternal(node);
    return node;
  }

  void touchNode(const std::shared_ptr<Node> &node) {
    ZoneScopedN("Physics::touch");

    auto it = nodeMap.find(node.get());
    shassert(it != nodeMap.end() && "Node not found");

    auto &nodeData = it->second;
    if (nodeData.lastTouched != frameCounter_) {
      nodeData.lastTouched = frameCounter_;
      if (node->enabled)
        activeNodes.push_back(&nodeData);
    }
  }

  void end() {
    ZoneScopedN("Physics::end");
    for (auto it = nodeMap.begin(); it != nodeMap.end(); ++it) {
      if (it->second.lastTouched != frameCounter_) {
        if (it->first->persistence && it->first->enabled) {
          activeNodes.push_back(&it->second);
        } else {
          if (it->second.bodyAdded && it->second.body) {
            physicsSystem.GetBodyInterface().RemoveBody(it->second.body->GetID());
            it->second.bodyAdded = false;
          }
        }
      }
    }

    for (auto &node : activeNodes) {
      auto &src = node->node;

      if (node->body) {
        if (src->paramHash0 != node->paramHash0) {
          auto mp = node->body->GetMotionProperties();
          node->body->SetRestitution(src->params.resitution);
          node->body->SetFriction(src->params.friction);
          mp->SetLinearDamping(src->params.linearDamping);
          mp->SetAngularDamping(src->params.angularDamping);
        }
        if (src->paramHash1 != node->paramHash1) {
          auto mp = node->body->GetMotionProperties();
          mp->SetMaxLinearVelocity(src->params.maxLinearVelocity);
          mp->SetMaxAngularVelocity(src->params.maxAngularVelocity);
          mp->SetGravityFactor(src->params.gravityFactor);
          mp->SetMassProperties(src->params.allowedDofs, src->shape->GetMassProperties());
          node->body->SetMotionType(src->params.motionType);
        }
        if (src->shapeUid != node->shapeUid) {
          physicsSystem.GetBodyInterface().SetShape(node->body->GetID(), src->shape.GetPtr(), true, JPH::EActivation::Activate);
          node->shapeUid = src->shapeUid;
        }
      }

      if (!node->body) {
        if (node->body) {
          physicsSystem.GetBodyInterface().RemoveBody(node->body->GetID());
          node->body = nullptr;
        }

        JPH::BodyCreationSettings settings(src->shape.GetPtr(), JPH::RVec3Arg(src->location), JPH::QuatArg(src->rotation),
                                           src->params.motionType, BroadPhaseLayers::MOVING.GetValue());
        settings.mFriction = src->params.friction;
        settings.mRestitution = src->params.resitution;
        settings.mLinearDamping = src->params.linearDamping;
        settings.mAngularDamping = src->params.angularDamping;
        settings.mMaxLinearVelocity = src->params.maxLinearVelocity;
        settings.mMaxAngularVelocity = src->params.maxAngularVelocity;
        settings.mGravityFactor = src->params.gravityFactor;
        settings.mAllowedDOFs = src->params.allowedDofs;
        settings.mMotionType = src->params.motionType;
        node->body = physicsSystem.GetBodyInterface().CreateBody(settings);
        node->paramHash0 = src->paramHash0;
        node->paramHash1 = src->paramHash1;
        node->shapeUid = src->shapeUid;
      }

      if (!node->bodyAdded) {
        physicsSystem.GetBodyInterface().AddBody(node->body->GetID(), JPH::EActivation::Activate);
        node->bodyAdded = true;
      }
    }
  }

  void simulate() {
    float dt = 1.0 / 60.0;
    physicsSystem.Update(dt, 2, &tempAllocator, &jobSystem);
  }

  JPH::PhysicsSystem &getPhysicsSystem() { return physicsSystem; }

private:
  void addNodeInternal(const std::shared_ptr<Node> &node) {
    ZoneScopedN("Physics::addNode");
    Node *key = node.get();
    auto it = nodeMap.emplace(key, AssociatedData{node}).first;
    // addedNodes.push_back(&it->second);
  }

  void removeNodeInternal(const std::shared_ptr<Node> &node) {
    auto it = nodeMap.find(node.get());
    shassert(it != nodeMap.end() && "Node not found");
    auto &nd = it->second;
    if (nd.body) {
      physicsSystem.GetBodyInterface().RemoveBody(nd.body->GetID());
      nd.body = nullptr;
    }
    nodeMap.erase(it);
  }
};

struct ShardsContext {
  static inline int32_t ObjectId = 'phCx';
  static inline const char VariableName[] = "Physics.Context";
  static inline shards::Type Type = Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline shards::Type VarType = Type::VariableOf(Type);

  static inline shards::ObjectVar<ShardsContext, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                            RawType.object.typeId};
  std::shared_ptr<Core> core;
};

using RequiredContext = RequiredContextVariable<ShardsContext, ShardsContext::RawType, ShardsContext::VariableName, true>;

} // namespace shards::Physics