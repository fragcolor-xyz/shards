#ifndef CDA366C4_E8D0_474B_AFA2_F66229C830BB
#define CDA366C4_E8D0_474B_AFA2_F66229C830BB

#include "foundation.hpp"
#include "pmr/wrapper.hpp"
#include "pmr/unordered_map.hpp"
#include "pmr/unordered_set.hpp"
#include "pmr/shared_temp_allocator.hpp"
#include "wire_runtime.hpp"
#include "variable_accessor.hpp"
#include <shards/iterator.hpp>
#include <spdlog/spdlog.h>

typedef void (*SHCAnnotatePasshtrough)(const SHInstanceData &);
typedef void (*SHCAnnotateVariableReference)(const SHInstanceData &, SHStringWithLen variable);
struct SHComposeInterface {
  SHCAnnotatePasshtrough annotatePasshtrough;
  SHCAnnotateVariableReference annotateVariableReference;
};

namespace shards {

namespace compose {
extern std::shared_ptr<spdlog::logger> logger;

inline const uint32_t InternalIdNone = 0;
inline const uint32_t InternalIdFlagsInternal = 1 << 31;
inline const uint32_t InternalIdFlagMask = InternalIdFlagsInternal;
inline const uint32_t InternalIdValueMask = InternalIdFlagsInternal - 1;

enum VariableKind {
  Local,
  External,
  Inherited,
  Global,
};

struct Variable {
  size_t id;
  size_t declaredIn;
  VariableKind kind;
  SHExposedTypeInfo exposed;
  // Optional reference target in case this variable is a reference
  std::optional<VariableAccessorChain> referenceTarget;
  // Number that keeps track of write access to the variable
  // used to invalidate references
  size_t referenceVersion;

  uint32_t internalId() const { return uint32_t(id) | InternalIdFlagsInternal; }
  bool isReference() const { return referenceTarget.has_value(); }
};

struct ComposedWire;
struct VariableRef {
  Variable *variable;

  VariableRef() : variable(nullptr) {}
  VariableRef(Variable *variable) : variable(variable) {}

  operator bool() const { return isValid(); }
  bool isValid() const { return variable; }
  const Variable *operator->() const { return variable; }
  Variable &mutate() { return *variable; }
};

struct ShardInfo {
  size_t seqId{};
  Shard *shard;
  std::vector<size_t> variableRefs;
};

struct TrackedReference {
  // The version that determines if the reference is still valid
  size_t version{};
  SHTypeInfo cachedType{};
  bool isMutable{};
  const ::shards::compose::ShardInfo *lastInvalidatedBy{};
};

struct ResolvedReferenceInfo {
  std::optional<SHExposedTypeInfo> exposedTypeInfo{};
  const SHTypeInfo *typeInfoPtr{};
  bool isMutable{};

  bool isValid() const { return typeInfoPtr != nullptr; }
  operator bool() const { return isValid(); }
};

struct VersionedReference {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;
  VariableAccessorChain chain;
  size_t version;

  VersionedReference(std::allocator_arg_t, allocator_type a, VariableAccessorChain chain, size_t version)
      : chain(std::allocator_arg, a, std::move(chain)), version(version) {}
  VersionedReference(allocator_type a, VariableAccessorChain &&chain, size_t version)
      : VersionedReference(std::allocator_arg, a, std::move(chain), version) {}

  template <typename T>
  VersionedReference(allocator_type a, T &&chain, size_t version)
      : chain(std::allocator_arg, a, std::forward<T>(chain)), version(version) {}
};

struct Scope {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;
  Scope(std::allocator_arg_t, allocator_type a);
  Scope(std::allocator_arg_t, allocator_type a, Scope &&other);

  size_t id;

  std::optional<VersionedReference> currentAccess;

  std::string_view shardName;

  // Keeps track of annotations on the shards added during compose
  // resets for every new shard
  struct ShardAnnotationState {
    // Used to determing if the shard uses flow instrumentation
    // if not, we can not determine the variable flo
    bool isAnnotated{};
    bool isPassthrough{};

    void reset() { *this = {}; }
  };
  ShardAnnotationState annotations;

  struct PerShard {};
  pmr::vector<PerShard> shards;

  pmr::unordered_map<std::string_view, size_t> variableMap;
  pmr::vector<size_t> usedVariables;

  // Determinies whenever to lookup parent scope variables or not
  bool hasAllVariables{};

  size_t estimatedNumVariables{};

  SHTypeInfo previousOutputType{};
  SHTypeInfo originalInputType{};

  Shard *bottom{};
  Shard *next{};
  std::shared_ptr<ComposedWire> wire;

  bool onWorkerThread{false};

  std::string_view wireName() const;

  void reset() {
    usedVariables.clear();
    variableMap.clear();
    shards.clear();
    bottom = nullptr;
    next = nullptr;
    wire = nullptr;
    onWorkerThread = false;
    previousOutputType = SHTypeInfo();
    originalInputType = SHTypeInfo();
  }
};

struct ComposedWire {
  std::unordered_map<std::string_view, SHExternalVariable> required;
  SHWire *source;

  std::unordered_map<size_t, ::shards::compose::ShardInfo> shardSeqId;

  ComposedWire(SHWire *source);
};

// Groups all child references that originate from a given root owned (usually variable)
// this make it easier to invalidate a whole reference tree
struct TrackedReferenceGroup {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;
  pmr::unordered_map<VariableAccessorChain, TrackedReference> map;

  TrackedReferenceGroup(std::allocator_arg_t, allocator_type a) : map(a) {}
  TrackedReferenceGroup(allocator_type a) : TrackedReferenceGroup(std::allocator_arg, a) {}
  TrackedReferenceGroup(std::allocator_arg_t, allocator_type a, TrackedReferenceGroup &&other) : map(std::move(other.map), a) {}
};

struct CompositionContext {
  pmr::SharedTempAllocator tempAllocator;
  shards::pmr::unordered_map<SHWire *, SHTypeInfo> visitedWires;
  std::vector<std::string> errorStack;
  shards::LayeredMap<std::string_view, SHExposedTypeInfo> inherited;

  shards::pmr::vector<compose::Scope *> scopePool;
  shards::pmr::vector<compose::Scope *> stack;
  shards::pmr::unordered_map<SHWire *, std::shared_ptr<compose::ComposedWire>> wires;

  std::vector<Variable> variables;

  // Contains all the created references
  pmr::unordered_map<VariableAccessor, TrackedReferenceGroup> rootReferenceMap;

  // Cached wire output types for reference tracking
  pmr::unordered_map<size_t, SHTypeInfo> wireOutputTypes;

  size_t idAllocator{};
  mutable std::string shardContextStrBuf;

  CompositionContext();
  ~CompositionContext();

  Scope &currentScope(size_t scopeOffset = 0);
  const Scope &currentScope(size_t scopeOffset = 0) const;
  [[deprecated("Use currentScope() instead")]] compose::Scope &current(size_t scopeOffset = 0) {
    return currentScope(scopeOffset);
  }

  ResolvedReferenceInfo resolveReferenceInfo(const VariableAccessorChain &va);
  TrackedReference &trackReference(const VariableAccessorChain &va);
  TrackedReference *resolveReference(const VariableAccessorChain &va);

  Shard *currentShard(size_t scopeOffset = 0) const;
  std::string_view currentShardName() const;
  std::string_view shardContextStr(size_t scopeOffset = 0) const;
  std::string_view shardContextStr(const Shard *shard) const;

  const char *tmpStr(std::string_view str);

  VariableRef findVariable(std::string_view name, size_t scopeOffset = 0);
  VariableRef findVariable(uint32_t id, size_t scopeOffset = 0);

  // The current accessor chain for the input variable
  const VersionedReference &currentAccess() const;

  // Insert a new variables
  VariableRef insertVariable(std::string_view name, SHExposedTypeInfo type);
  // Create a new reference variable
  VariableRef insertRefVariable(std::string_view name, const VersionedReference &vr, bool isMutable = false,
                                std::optional<SHTypeInfo> exposeAs = std::nullopt);
  VariableRef insertAnonymousVariable(SHExposedTypeInfo type);

  ::shards::compose::ShardInfo &currentShardInfo();

  void assignWireComposeID(SHWire *wire);

  compose::Scope &pushScope(std::optional<SHTypeInfo> inputType = std::nullopt);
  void popScope();
  // Annotates a sub-path of an input variable (e.g. array index or table key)
  // The magic key none is reserved for dynamic keys in table context
  void annotateSubPath(const SHVar &key);
  void annotateRef(const VariableRef &variable);
  TrackedReference &annotateReferenceTo(const VariableAccessorChain &va);
  // Clears the currently tracked variable
  void annotateClearVariable();
  // Annotates that the current shards is pass-through
  void annotatePassthrough();
  void annotateRebaseFlow();
  // Annotates that the current shard is outputting the output of a wire
  void annotateWireOutput(SHWire *wire);
  void invalidateWireOutput(SHWire *wire);

  enum InvalidationSource {
    CurrentShard,
    ScopeExit,
    Manual,
  };

  void invalidateReferencePath(const VariableAccessorChain &va, InvalidationSource is = InvalidationSource::CurrentShard);
  // void invalidateVariable(const VariableRef &variable);
  void invalidateExposedReferences(const SHExposedTypesInfo &eti);

  VariableAccessorChain getPathToVariable(const VariableRef &variable);

  // Checks if a reference variable is valid at this point in time
  void checkReferenceIsValid(VariableAccessorChain chain, size_t version);
  void checkReferenceIsValidInternal(TrackedReference *reference, VariableAccessorChain chain, size_t version);

  // Temporary
  SHTypeInfo &previousOutputType() { return currentScope().previousOutputType; }

  void step();

  shards::pmr::PolymorphicAllocator<> getAllocator() { return tempAllocator.getAllocator(); }

  static CompositionContext &get(const SHInstanceData &data) {
    shassert(data.privateContext && "Private context should be valid");
    return *reinterpret_cast<CompositionContext *>(data.privateContext);
  }

private:
  void flowTagPassthrough() { currentScope().annotations.isPassthrough = true; }
  void flowTagAnnotated() { currentScope().annotations.isAnnotated = true; }

  // Internal, annotates the next shard to be processed
  void flowAnnotateNextShard(Shard *);
  void flowClearVariable();
  void flowAppendVA(VariableAccessor va);
  void flowClearUndeterministic();
};

bool typeRequiresInvalidationWhenUpdated(const SHTypeInfo& type);
inline bool typesRequireInvalidationWhenUpdated(const SHTypesInfo& types) {
  for(auto& t : types) {
    if (typeRequiresInvalidationWhenUpdated(t)) {
      return true;
    }
  }
  return false;
}

} // namespace compose
using compose::CompositionContext;
} // namespace shards
struct SHPrivateContext : public shards::CompositionContext {};
#endif /* CDA366C4_E8D0_474B_AFA2_F66229C830BB */
