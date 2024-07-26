
#include "physics.hpp"
#include "job_system.hpp"
#include "broadphase.hpp"
#include <shards/core/pmr/vector.hpp>
#include <shards/core/pmr/temp_allocator.hpp>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

namespace shards::Physics {
static inline auto logger = getLogger();

enum class EventType {
  ContactAdded,
  ContactPersisted,
};
struct Event {
  EventType et;
  const JPH::Body *body0;
  const JPH::Body *body1;
  JPH::ContactManifold manifold;
};

struct GroupFilter : JPH::GroupFilter {
  bool CanCollide(const JPH::CollisionGroup &inGroup1, const JPH::CollisionGroup &inGroup2) const override {
    auto f0Memb = inGroup1.GetGroupID();
    auto f0Mask = inGroup1.GetSubGroupID();
    auto f1Memb = inGroup2.GetGroupID();
    auto f1Mask = inGroup2.GetSubGroupID();
    return (f0Memb & f1Mask) && (f1Memb & f0Mask);
  }
  static const JPH::Ref<GroupFilter> &instance() {
    static JPH::Ref<GroupFilter> gf = new GroupFilter();
    return gf;
  }
};

struct AssociatedData {
  std::shared_ptr<Node> node;
  size_t lastTouched{};

  JPH::Body *body{};
  bool bodyAdded{};

  // Iterator helper
  AssociatedData *next{};

  uint64_t paramHash0;
  uint64_t paramHash1;
  uint64_t shapeUid;

  std::optional<pmr::vector<Event *>> events;
};

struct EventCollector {
  struct ThreadBuffer {
    // Used for collision events
    pmr::TempAllocator tempAllocator;
    std::optional<pmr::vector<Event>> events;

    ThreadBuffer() { events.emplace(&tempAllocator); }
    ThreadBuffer(const ThreadBuffer &) = delete;

    void clear() {
      events.reset();
      tempAllocator.reset();
      events.emplace(&tempAllocator);
    }
  };

  pmr::TempAllocator tempAllocator;
  std::map<std::thread::id, ThreadBuffer> threadBuffers;
  std::shared_mutex mutex;

  EventCollector() {}
  EventCollector(EventCollector &&) = delete;

  ThreadBuffer &getBuffer() {
    auto id = std::this_thread::get_id();
    std::shared_lock lock(mutex);
    auto it = threadBuffers.find(id);
    if (it != threadBuffers.end())
      return it->second;
    lock.unlock();
    std::unique_lock ulock(mutex);
    return threadBuffers.emplace(std::piecewise_construct, std::make_tuple(id), std::make_tuple()).first->second;
  }

  void clear() {
    tempAllocator.reset();
    for (auto &tb : threadBuffers) {
      tb.second.clear();
    }

    // TODO: Fix lazy GC
    //  technically okay since we schedule on the same thread pool anyways
    if (threadBuffers.size() > 64)
      threadBuffers.clear();
  }
};

struct ContactListener : public JPH::ContactListener {
  EventCollector *evtCollector;

  void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold,
                      JPH::ContactSettings &ioSettings) override {
    auto &buffer = evtCollector->getBuffer();
    buffer.events->push_back(Event{EventType::ContactAdded, &inBody1, &inBody2, inManifold});
  }

  void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold,
                          JPH::ContactSettings &ioSettings) override {
    auto &buffer = evtCollector->getBuffer();
    buffer.events->push_back(Event{EventType::ContactPersisted, &inBody1, &inBody2, inManifold});
  }

  void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override {
    // Noop, removal detected by absence of added/persisted events
  }
};

struct Core : public std::enable_shared_from_this<Core> {
  using MapType = boost::container::flat_map<Node *, AssociatedData, std::less<Node *>, //
                                             boost::container::stable_vector<std::pair<Node *, AssociatedData>>>;

private:
  static std::atomic_uint64_t UidCounter;
  uint64_t uid = UidCounter++;

  MapType nodeMap;
  boost::container::flat_map<const JPH::Body *, AssociatedData *> bodyMap;

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
  EventCollector eventCollector;
  ContactListener contactListener;

public:
  Core() { contactListener.evtCollector = &eventCollector; }

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
            it->second.events.reset();
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

          SPDLOG_LOGGER_TRACE(logger, "Updated params0 for node {}", (void *)node);
        }
        if (src->paramHash1 != node->paramHash1) {
          auto mp = node->body->GetMotionProperties();
          mp->SetMaxLinearVelocity(src->params.maxLinearVelocity);
          mp->SetMaxAngularVelocity(src->params.maxAngularVelocity);
          mp->SetGravityFactor(src->params.gravityFactor);
          mp->SetMassProperties(src->params.allowedDofs, src->shape->GetMassProperties());
          node->body->SetMotionType(src->params.motionType);
          JPH::CollisionGroup group;
          group.SetGroupFilter(GroupFilter::instance());
          group.SetGroupID(src->params.groupMembership);
          group.SetSubGroupID(src->params.collisionMask);
          node->body->SetCollisionGroup(group);

          SPDLOG_LOGGER_TRACE(logger, "Updated params1 for node {}", (void *)node);
        }
        if (src->shapeUid != node->shapeUid) {
          physicsSystem.GetBodyInterface().SetShape(node->body->GetID(), src->shape.GetPtr(), true, JPH::EActivation::Activate);
          node->shapeUid = src->shapeUid;

          SPDLOG_LOGGER_TRACE(logger, "Updated shape for node {}", (void *)node);
        }
      }

      if (!node->body) {
        if (node->body) {
          bodyRemoved(node->body);
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
        settings.mIsSensor = src->params.sensor;
        settings.mCollisionGroup.SetGroupFilter(GroupFilter::instance());
        settings.mCollisionGroup.SetGroupID(src->params.groupMembership);
        settings.mCollisionGroup.SetSubGroupID(src->params.collisionMask);
        node->body = physicsSystem.GetBodyInterface().CreateBody(settings);
        node->paramHash0 = src->paramHash0;
        node->paramHash1 = src->paramHash1;
        node->shapeUid = src->shapeUid;

        SPDLOG_LOGGER_TRACE(logger, "Created body for node {}", (void *)node);
      }

      if (!node->bodyAdded) {
        physicsSystem.GetBodyInterface().AddBody(node->body->GetID(), JPH::EActivation::Activate);
        node->bodyAdded = true;
        bodyAdded(node->body, node);
      }
    }
  }

  void bodyAdded(JPH::Body *body, AssociatedData *data) { bodyMap.emplace(body, data); }
  void bodyRemoved(JPH::Body *body) { bodyMap.erase(body); }

  AssociatedData *getAssociatedData(const JPH::Body *body) const {
    auto it = bodyMap.find(body);
    if (it != bodyMap.end())
      return it->second;
    return nullptr;
  }

  void simulate(double dt, int numIterations) {
    eventCollector.clear();
    physicsSystem.SetContactListener(&contactListener);
    physicsSystem.Update(dt, numIterations, &tempAllocator, &jobSystem);

    pmr::unordered_map<const JPH::Body *, pmr::vector<Event *>> eventMap(&eventCollector.tempAllocator);
    for (auto &buf : eventCollector.threadBuffers) {
      for (auto &event : buf.second.events.value()) {
        auto &events = eventMap[event.body0];
        events.push_back(&event);
        auto &events1 = eventMap[event.body1];
        events1.push_back(&event);
      }
    }

    // Update nodes
    for (auto &node : activeNodes) {
      auto &src = node->node;
      auto &body = node->body;

      src->location = body->GetPosition();
      src->rotation = body->GetRotation();
      node->events.reset();
      if (src->neededCollisionEvents > 0) {
        auto it = eventMap.find(node->body);
        if (it != eventMap.end()) {
          auto &dstEvents = node->events.emplace(&eventCollector.tempAllocator);
          for (auto &evt : it->second) {
            dstEvents.push_back(evt);
          }
        }
      }
    }
  }

  JPH::PhysicsSystem &getPhysicsSystem() { return physicsSystem; }

private:
  void addNodeInternal(const std::shared_ptr<Node> &node) {
    ZoneScopedN("Physics::addNode");
    Node *key = node.get();
    auto it = nodeMap.emplace(key, AssociatedData{node}).first;
    node->data = &it->second;
  }

  void removeNodeInternal(const std::shared_ptr<Node> &node) {
    auto it = nodeMap.find(node.get());
    shassert(it != nodeMap.end() && "Node not found");
    auto &nd = it->second;
    if (nd.body) {
      bodyRemoved(nd.body);
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