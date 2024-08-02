
#include "physics.hpp"
#include "job_system.hpp"
#include "broadphase.hpp"
#include "constraints.hpp"
#include <shards/core/pmr/vector.hpp>
#include <shards/core/pmr/temp_allocator.hpp>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <concepts>

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

template <typename K, typename V>
using StableMap = boost::container::flat_map<K, V, std::less<K>, boost::container::stable_vector<std::pair<K, V>>>;

template <typename T>
concept MirrorableType = requires(const T &t) {
  { t.uid } -> std::convertible_to<uint64_t>;
  { t.enabled } -> std::convertible_to<bool>;
  { t.persistence } -> std::convertible_to<bool>;
};

template <typename T, typename TData>
concept MirrorableType_DataPtr = requires(T &t, TData *dataPtr) {
  { t.data = dataPtr };
};

template <typename T>
concept AssociatedData = requires(T &t, JPH::PhysicsSystem *psys) {
  { t.node.reset() };
  { t.node = nullptr };
  { t.getPhysicsObject() };
  { t.getPhysicsObjectAdded() } -> std::convertible_to<bool>;
  t.lastTouched = uint64_t(0);
};

template <typename T, typename TData>
concept MirrorInterface_Base = requires(T &t, TData &data) {
  { t.createPhysicsObject(data) };
  { t.addToPhysics(data) };
  { t.removeFromPhysics(data) };
};

template <typename T, typename TData>
concept MirrorInterface_IsValid = requires(T &t, TData &data) {
  { t.isValid(data) } -> std::convertible_to<bool>;
};

// Mirrors creation of jolt-side object and updating of parameters
template <MirrorableType T, AssociatedData TData> struct PhysicsMirror {
  using PtrType = std::shared_ptr<T>;
  using PhysicsObjectType = std::remove_pointer_t<decltype(std::declval<TData>().getPhysicsObject())>;

  StableMap<uint64_t, TData> map;
  boost::container::flat_map<const PhysicsObjectType *, TData *> pobjMap;
  std::vector<TData *> active;
  std::vector<TData *> pendingRemove;

  void begin() { active.clear(); }

  void touchActive(const PtrType &node, uint64_t frameCounter) {
    ZoneScoped;
    auto it = map.find(node->uid);
    shassert(it != map.end() && "Object not found");

    auto &nodeData = it->second;
    if (nodeData.lastTouched != frameCounter) {
      nodeData.lastTouched = frameCounter;
      if (node->enabled)
        active.push_back(&nodeData);
    }
  }

  TData *findAssociatedData(const PhysicsObjectType *ptype) const {
    auto it = pobjMap.find(ptype);
    if (it != pobjMap.end())
      return it->second;
    return nullptr;
  }

  TData *findAssociatedData(const uint64_t nodeId) const {
    auto it = map.find(nodeId);
    if (it != map.end())
      return const_cast<TData *>(&it->second);
    return nullptr;
  }

  void addNode(const PtrType &node) {
    ZoneScopedN("Physics::addNode");
    if constexpr (MirrorableType_DataPtr<T, TData>) {
      shassert(!node->data);
      auto &data = map[node->uid];
      data.node = node;
      node->data = &data;
    } else {
      auto &data = map[node->uid];
      data.node = node;
    }
  }

  void removeNode(const PtrType &node) {
    if constexpr (MirrorableType_DataPtr<T, TData>) {
      shasert(node->data);
      node->data = nullptr;
    }
    pendingRemove.push_back(node->data);
  }

  template <MirrorInterface_Base<TData> TInterface> void updateSet(TInterface &mirrorInterface, uint64_t frameCounter) {
    ZoneScoped;

    for (auto &data : pendingRemove) {
      SPDLOG_LOGGER_TRACE(logger, "Processing node removal {}", (void *)data);
      auto pobj = data->getPhysicsObject();
      if (pobj) {
        pobjMap.erase(pobj);
      }
      if (data->getPhysicsObjectAdded()) {
        mirrorInterface.removeFromPhysics(*data);
      }
      map.erase(data->node->uid);
    }
    pendingRemove.clear();

    // Add persistent bodies and remove inactive bodies
    for (auto it = map.begin(); it != map.end(); ++it) {
      auto &data = it->second;
      if (data.lastTouched != frameCounter) {
        auto &node = data.node;
        if (node->persistence && node->enabled) {
          active.push_back(&data);
        } else {
          if (data.getPhysicsObjectAdded()) {
            mirrorInterface.removeFromPhysics(data);
          }
        }
      }
    }

    // Create new bodies and update body params
    for (auto &dataPtr : active) {
      TData &data = *dataPtr;

      if constexpr (MirrorInterface_IsValid<TInterface, TData>) {
        bool valid = mirrorInterface.isValid(data);
        if (valid) {
          if (!data.getPhysicsObject()) { // Create new physics object
            mirrorInterface.createPhysicsObject(data);
            shassert(data.getPhysicsObject());
            pobjMap.emplace(data.getPhysicsObject(), &data);
          }
          if (!data.getPhysicsObjectAdded()) {
            mirrorInterface.addToPhysics(data);
          }
          mirrorInterface.maybeUpdateParams(data);
        } else if (!valid && data.getPhysicsObjectAdded()) {
          mirrorInterface.removeFromPhysics(data);
        }
      } else {
        if (!data.getPhysicsObject()) { // Create new physics object
          mirrorInterface.createPhysicsObject(data);
          shassert(data.getPhysicsObject());
          pobjMap.emplace(data.getPhysicsObject(), &data);
        }
        if (!data.getPhysicsObjectAdded()) {
          mirrorInterface.addToPhysics(data);
        }
        mirrorInterface.maybeUpdateParams(data);
      }
    }
  }
};

struct BodyAssociatedData {
  std::shared_ptr<BodyNode> node;
  uint64_t lastTouched{};

  // The cached physics body
  JPH::Body *body{};
  // Sometimes body is still cached but removed from the physics system temporarily
  //  because maybe the node was disabled
  bool bodyAdded{};

  // Hashes of the currently applied BodyNode Params & Shape, to detect changes quickly
  uint64_t paramHash0;
  uint64_t paramHash1;
  uint64_t shapeUid;

  // Stores a event array only valid for the current frame
  std::optional<pmr::vector<Event *>> events;

  JPH::Body *getPhysicsObject() const { return body; }
  bool getPhysicsObjectAdded() const { return bodyAdded; }
};

struct IBodyMirror {
  JPH::PhysicsSystem *physicsSystem;
  JPH::BodyInterface &bodyInterface;

  // bool isValid(const BodyAssociatedData &data) const { return true; }
  void createPhysicsObject(BodyAssociatedData &data) {
    auto &node = data.node;
    if (node->shape.index() == 0) {
      auto &inParams = node->params.regular;
      auto bpl = inParams.motionType == JPH::EMotionType::Static ? BroadPhaseLayers::NON_MOVING.GetValue()
                                                                 : BroadPhaseLayers::MOVING.GetValue();
      JPH::BodyCreationSettings settings(std::get<0>(node->shape).GetPtr(), JPH::RVec3Arg(node->location),
                                         JPH::QuatArg(node->rotation), inParams.motionType, bpl);
      settings.mFriction = inParams.friction;
      settings.mRestitution = inParams.restitution;
      settings.mLinearDamping = inParams.linearDamping;
      settings.mAngularDamping = inParams.angularDamping;
      settings.mMaxLinearVelocity = inParams.maxLinearVelocity;
      settings.mMaxAngularVelocity = inParams.maxAngularVelocity;
      settings.mGravityFactor = inParams.gravityFactor;
      settings.mAllowedDOFs = inParams.allowedDofs;
      settings.mMotionType = inParams.motionType;
      settings.mIsSensor = inParams.sensor;
      settings.mCollisionGroup.SetGroupFilter(GroupFilter::instance());
      settings.mCollisionGroup.SetGroupID(inParams.groupMembership);
      settings.mCollisionGroup.SetSubGroupID(inParams.collisionMask);
      settings.mAllowSleeping = false; // TODO: Param
      settings.mCollideKinematicVsNonDynamic = true; // TODO: Param
      data.body = bodyInterface.CreateBody(settings);
      data.paramHash0 = node->paramHash0;
      data.paramHash1 = node->paramHash1;
      data.shapeUid = node->shapeUid;
      SPDLOG_LOGGER_TRACE(logger, "Created body for node {}", (void *)&data);
    } else {
      auto &inParams = node->params.soft;
      JPH::SoftBodyCreationSettings settings(std::get<1>(node->shape).GetPtr(), JPH::RVec3Arg(node->location),
                                             JPH::QuatArg(node->rotation), BroadPhaseLayers::MOVING.GetValue());
      settings.mFriction = inParams.friction;
      settings.mRestitution = inParams.restitution;
      settings.mLinearDamping = inParams.linearDamping;
      settings.mMaxLinearVelocity = inParams.maxLinearVelocity;
      settings.mGravityFactor = inParams.gravityFactor;
      settings.mPressure = inParams.pressure;
      settings.mCollisionGroup.SetGroupFilter(GroupFilter::instance());
      settings.mCollisionGroup.SetGroupID(inParams.groupMembership);
      settings.mCollisionGroup.SetSubGroupID(inParams.collisionMask);
      data.body = bodyInterface.CreateSoftBody(settings);
      data.paramHash0 = node->paramHash0;
      data.paramHash1 = node->paramHash1;
      data.shapeUid = node->shapeUid;
      SPDLOG_LOGGER_TRACE(logger, "Created soft body for node {}", (void *)&data);
    }
  }

  void addToPhysics(BodyAssociatedData &data) {
    bodyInterface.AddBody(data.body->GetID(), JPH::EActivation::Activate);
    data.bodyAdded = true;
  }

  void removeFromPhysics(BodyAssociatedData &data) {
    bodyInterface.RemoveBody(data.body->GetID());
    data.bodyAdded = false;
    data.events.reset();
  }

  void maybeUpdateParams(BodyAssociatedData &data) {
    if (data.body->IsSoftBody())
      return;

    auto &node = data.node;
    auto &inParams = node->params.regular;
    if (node->paramHash0 != data.paramHash0) {
      auto mp = data.body->GetMotionProperties();
      data.body->SetRestitution(inParams.restitution);
      data.body->SetFriction(inParams.friction);
      mp->SetLinearDamping(inParams.linearDamping);
      mp->SetAngularDamping(inParams.angularDamping);
      data.paramHash0 = node->paramHash0;

      SPDLOG_LOGGER_TRACE(logger, "Updated params0 for node {}", (void *)&data);
    }

    if (node->paramHash1 != data.paramHash1) {
      auto mp = data.body->GetMotionProperties();
      mp->SetMaxLinearVelocity(inParams.maxLinearVelocity);
      mp->SetMaxAngularVelocity(inParams.maxAngularVelocity);
      mp->SetGravityFactor(inParams.gravityFactor);
      mp->SetMassProperties(inParams.allowedDofs, std::get<0>(node->shape)->GetMassProperties());
      data.body->SetMotionType(inParams.motionType);
      JPH::CollisionGroup group;
      group.SetGroupFilter(GroupFilter::instance());
      group.SetGroupID(inParams.groupMembership);
      group.SetSubGroupID(inParams.collisionMask);
      data.body->SetCollisionGroup(group);
      data.paramHash1 = node->paramHash1;

      SPDLOG_LOGGER_TRACE(logger, "Updated params1 for node {}", (void *)&data);
    }

    if (node->shapeUid != data.shapeUid) {
      bodyInterface.SetShape(data.body->GetID(), std::get<0>(node->shape).GetPtr(), true, JPH::EActivation::Activate);
      data.shapeUid = node->shapeUid;

      SPDLOG_LOGGER_TRACE(logger, "Updated shape for node {}", (void *)&data);
    }
  }
};

struct ConstraintAssociatedData {
  std::shared_ptr<ConstraintNode> node;
  uint64_t lastTouched{};

  JPH::Ref<JPH::Constraint> constraint{};
  bool constraintAdded{};

  uint64_t paramHash0;

  JPH::Constraint *getPhysicsObject() const { return constraint.GetPtr(); }
  bool getPhysicsObjectAdded() const { return constraintAdded; }
};

struct IConstraintMirror {
  JPH::PhysicsSystem *physicsSystem;
  PhysicsMirror<BodyNode, BodyAssociatedData> &bodyMirror;

  JPH::Body &getBody(const uint64_t bodyNodeUid) {
    if (bodyNodeUid == ~0) {
      return JPH::Body::sFixedToWorld;
    }

    auto bodyData = bodyMirror.findAssociatedData(bodyNodeUid);
    shassert(bodyData && "BodyNode not found");
    return *bodyData->body;
  }

  void createPhysicsObject(ConstraintAssociatedData &data) {
    auto &node = data.node;

    auto idA = data.node->params->bodyNodeIdA;
    auto idB = data.node->params->bodyNodeIdB;

    data.constraint = node->params->getSettings()->Create(getBody(idA), getBody(idB));
    data.paramHash0 = node->paramHash0;
  }

  bool isNodeIdValid(const uint64_t nodeId) const { return (nodeId == ~0) || bodyMirror.findAssociatedData(nodeId); }
  bool isValid(const ConstraintAssociatedData &data) const {
    auto &node = data.node;
    auto idA = node->params->bodyNodeIdA;
    auto idB = node->params->bodyNodeIdB;
    return isNodeIdValid(idA) && isNodeIdValid(idB);
  }

  void addToPhysics(ConstraintAssociatedData &data) {
    physicsSystem->AddConstraint(data.constraint);
    data.constraintAdded = true;
  }

  void removeFromPhysics(ConstraintAssociatedData &data) {
    physicsSystem->RemoveConstraint(data.constraint);
    data.constraintAdded = false;
  }

  void maybeUpdateParams(ConstraintAssociatedData &data) {
    auto &node = data.node;
    if (node->paramHash0 != data.paramHash0) {
      data.paramHash0 = node->paramHash0;
    }
  }
};

struct Core : public std::enable_shared_from_this<Core> {
private:
  PhysicsMirror<BodyNode, BodyAssociatedData> bodyMirror;
  PhysicsMirror<ConstraintNode, ConstraintAssociatedData> constraintMirror;

  uint64_t frameCounter_{};

  JPH::PhysicsSystem physicsSystem;

  JPH::TempAllocatorImpl tempAllocator{1024 * 1024 * 4};
  TidePoolJobSystem jobSystem{};

  BPLayerInterface broadPhaseLayerInterface;
  JPH::ObjectVsBroadPhaseLayerFilter objBroadPhaseFilter;
  JPH::ObjectLayerPairFilter objLayerPairFilter;
  EventCollector eventCollector;
  ContactListener contactListener;

public:
  Core() { contactListener.evtCollector = &eventCollector; }

  void begin() {
    ++frameCounter_;

    bodyMirror.begin();
    constraintMirror.begin();
  }

  const auto &getBodyNodeMap() const { return bodyMirror.map; }
  const auto &getActiveBodyNodes() const { return bodyMirror.active; }

  size_t frameCounter() const { return frameCounter_; }

  void init() { physicsSystem.Init(1024 * 8, 0, 1024, 1024, broadPhaseLayerInterface, objBroadPhaseFilter, objLayerPairFilter); }

  std::shared_ptr<BodyNode> createNewBodyNode() {
    auto node = std::make_shared<BodyNode>();
    bodyMirror.addNode(node);
    return node;
  }

  template <typename TParams> std::shared_ptr<ConstraintNode> createNewConstraintNode() {
    auto node = ConstraintNode::create<TParams>();
    constraintMirror.addNode(node);
    return node;
  }

  void touchNode(const std::shared_ptr<BodyNode> &node) {
    ZoneScopedN("Physics::touchBody");
    bodyMirror.touchActive(node, frameCounter_);
  }

  void touchNode(const std::shared_ptr<ConstraintNode> &node) {
    ZoneScopedN("Physics::touchConstraint");
    constraintMirror.touchActive(node, frameCounter_);
  }

  void end() {
    ZoneScopedN("Physics::end");

    IBodyMirror i0{&physicsSystem, physicsSystem.GetBodyInterface()};
    bodyMirror.updateSet(i0, frameCounter_);

    IConstraintMirror i1{&physicsSystem, bodyMirror};
    constraintMirror.updateSet(i1, frameCounter_);
  }

  BodyAssociatedData *findBodyAssociatedData(const JPH::Body *body) const { return bodyMirror.findAssociatedData(body); }

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
    for (auto &node : bodyMirror.active) {
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
};

struct ShardsContext {
  static inline int32_t ObjectId = 'phCx';
  static inline const char VariableName[] = "Physics.Context";
  static inline ::shards::Type Type = ::shards::Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline ::shards::Type VarType = ::shards::Type::VariableOf(Type);

  static inline shards::ObjectVar<ShardsContext, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                            RawType.object.typeId};
  std::shared_ptr<Core> core;
};

using RequiredContext = RequiredContextVariable<ShardsContext, ShardsContext::RawType, ShardsContext::VariableName, true>;

} // namespace shards::Physics