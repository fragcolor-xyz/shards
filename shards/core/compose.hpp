#ifndef CDA366C4_E8D0_474B_AFA2_F66229C830BB
#define CDA366C4_E8D0_474B_AFA2_F66229C830BB

#include "foundation.hpp"
#include "pmr/wrapper.hpp"
#include "pmr/unordered_map.hpp"
#include "pmr/unordered_set.hpp"
#include "pmr/shared_temp_allocator.hpp"
#include <boost/core/span.hpp>
#include "variable_accessor.hpp"
#include <spdlog/spdlog.h>

typedef void (*SHCAnnotatePasshtrough)(const SHInstanceData &);
typedef void (*SHCAnnotateVariableReference)(const SHInstanceData &, SHStringWithLen variable);
struct SHComposeInterface {
  SHCAnnotatePasshtrough annotatePasshtrough;
  SHCAnnotateVariableReference annotateVariableReference;
};

namespace shards {

struct WireRuntimeVariableInfo {
  static inline const uint32_t IdNone = 0;
  static inline const uint32_t IdFlagsExternal = 1 << 31;
  static inline const uint32_t IdFlagsInherited = 1 << 30;
  static inline const uint32_t IdFlagsGlobal = 1 << 29;
  static inline const uint32_t IdFlagMask = IdFlagsExternal | IdFlagsInherited | IdFlagsGlobal;
  static inline const uint32_t IdValueMask = IdFlagsGlobal - 1;
  // External + inherited
  std::vector<SHVar *> externalRefs;
  // Local only
  std::vector<SHVar> localVariableStorage;

  struct VariableDecl {
    std::string name;
    TypeInfo type;
  };

  // External + inherited
  std::vector<VariableDecl> externalAndInheritedVariables;
  // Local
  std::vector<VariableDecl> localVariables;
  // Global
  std::vector<VariableDecl> globalVariables;
  // External
  size_t numExternalVariables;

  struct VariableScope {
    std::unordered_map<std::string_view, size_t> variableLookup;
  };
  // Maps shard sequence id to variable a given variable scope
  // sorted for bounds search
  std::map<size_t, VariableScope> variableScopes;

  size_t numInheritedVariables() const { return externalAndInheritedVariables.size() - numExternalVariables; }

  WireRuntimeVariableInfo() = default;
  WireRuntimeVariableInfo(const WireRuntimeVariableInfo &) = delete;
  WireRuntimeVariableInfo &operator=(const WireRuntimeVariableInfo &) = delete;
  // WireRuntimeVariableInfo(WireRuntimeVariableInfo && other) = default;
  ~WireRuntimeVariableInfo() { cleanupStorage(); }

  void cleanupStorage() {
    // for (auto &v : localVariableStorage) {
    //   releaseVariable(&v);
    // }
    // localVariableStorage.clear();
  }

  void initStorage() {
    externalRefs.resize(externalAndInheritedVariables.size());
    localVariableStorage.resize(localVariables.size());
    for (auto &v : localVariableStorage) {
      // Make them ref-counted
      v.flags = SHVAR_FLAGS_REF_COUNTED;
      v.refcount = 0;
    }
  }

  SHVar *variableFromId(size_t vid) {
    if (vid & IdFlagsExternal) {
      return externalRefs[vid & IdValueMask];
    } else if (vid & IdFlagsInherited) {
      return externalRefs[vid & IdValueMask];
    } else {
      shassert((vid & IdFlagsGlobal) == 0 && "Global variables are not stored in wire storage");
      shassert(vid < localVariables.size() && "Invalid local variable id");
      return &localVariableStorage[vid];
    }
  }

  SHVar *findReferenceStrict(Shard *shard, std::string_view name) {
    auto it = variableScopes.find(shard->seqId);
    if (it == variableScopes.end())
      return nullptr;
    auto search = it->second.variableLookup.find(name);
    if (search == it->second.variableLookup.end())
      return nullptr;
    return variableFromId(search->second);
  }

  SHVar *findReference(Shard *shard, std::string_view name) {
    // Start at or just after shard->seqId
    auto it = variableScopes.lower_bound(shard->seqId);

    // Handle empty case
    if (variableScopes.empty())
      return nullptr;

    // If we got something past shard->seqId and it's not the first element, move back one
    if (it == variableScopes.end() || (it->first > shard->seqId && it != variableScopes.begin()))
      --it;

    // Now search through scopes from this point backward
    while (true) {
      auto &scope = it->second;
      auto search = scope.variableLookup.find(name);
      if (search != scope.variableLookup.end()) {
        return variableFromId(search->second);
      }

      if (it == variableScopes.begin())
        break;
      --it;
    }
    return nullptr;
  }
};

namespace compose {
extern std::shared_ptr<spdlog::logger> logger;

inline const uint32_t InternalIdNone = 0;
inline const uint32_t InternalIdFlagsInternal = 1 << 31;
inline const uint32_t InternalIdFlagMask = InternalIdFlagsInternal;
inline const uint32_t InternalIdValueMask = InternalIdFlagsInternal - 1;

// Matches a `x IsXXX` or `x Is(@type(...))` rule
struct AssertionRule_IsA {
  flow::VariableAccessorChain target;
  TypeInfo type;
};

struct AssertionRule_IsNot {
  flow::VariableAccessorChain target;
  TypeInfo type;
};

struct AssertionRule : public std::variant<AssertionRule_IsA, AssertionRule_IsNot> {
  using variant::variant;

  std::optional<AssertionRule> inverted() const {
    if (auto r = std::get_if<AssertionRule_IsA>(this)) {
      return AssertionRule_IsNot{r->target, TypeInfo(*r->type)};
    } else if (auto r = std::get_if<AssertionRule_IsNot>(this)) {
      return AssertionRule_IsA{r->target, TypeInfo(*r->type)};
    } else {
      return std::nullopt;
    }
  }
};

struct AssertionRules {
  std::vector<AssertionRule> matches;

  void clear();
};

struct FlowAnalysis {
  std::optional<SHTypeInfo> inputType;
  std::optional<SHTypeInfo> currentVariableType;
  std::optional<flow::VariableAccessorChain> currentAccess;

  std::string_view shardName;
  size_t shardIndex{};

  // Accumulated assertion rules like:
  //  IsString
  // or
  //  IsNotNone
  // they will get added here as long as the output value represents a boolean asserting the condition
  std::optional<AssertionRule> stackRule;

  // Rules that are commited using And chaining
  AssertionRules commitedRules;

  // Keeps track of annotations on the shards added during compose
  // resets for every new shard
  struct ShardAnnotationState {
    // Used to determing if the shard uses flow instrumentation
    // if not, we can not determine the variable flo
    bool isInstrumented{};
    bool isPassthrough{};

    void reset() { *this = {}; }
  };
  ShardAnnotationState annotations;
};

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
  bool isMutable : 1;
  bool isReference : 1;
};

struct ComposedWire;
struct VariableRef {
  Variable *variable;
  ComposedWire *wire;

  operator bool() { return isValid(); }
  bool isValid() const { return variable; }
  const Variable *operator->() const { return variable; }
};

struct Scope {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;
  Scope(std::allocator_arg_t, allocator_type a);
  Scope(std::allocator_arg_t, allocator_type a, Scope &&other);

  size_t id;

  FlowAnalysis flow;

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

  struct ShardInfo {
    size_t seqId{};
    Shard *shard;
    std::vector<size_t> variableRefs;
  };
  std::unordered_map<size_t, ShardInfo> shardSeqId;

  ComposedWire(SHWire *source);
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

  size_t idAllocator{};
  mutable std::string shardContextStrBuf;

  CompositionContext();
  ~CompositionContext();

  Scope &currentScope(size_t scopeOffset = 0);
  const Scope &currentScope(size_t scopeOffset = 0) const;

  Shard *currentShard(size_t scopeOffset = 0) const;
  std::string_view currentShardName() const;
  std::string_view shardContextStr(size_t scopeOffset = 0) const;
  std::string_view shardContextStr(Shard *shard) const;

  VariableRef findVariable(std::string_view name, size_t scopeOffset = 0);
  VariableRef findVariable(uint32_t id, size_t scopeOffset = 0);

  Variable &insertVariable(std::string_view name, SHExposedTypeInfo type);
  Variable &insertAnonymousVariable(SHExposedTypeInfo type);

  ComposedWire::ShardInfo &currentShardInfo();

  compose::Scope &pushScope(std::optional<SHTypeInfo> inputType = std::nullopt);
  void popScope();
  // Push a context variable to the analyzer
  void annotateContextVariable(std::string_view name, std::optional<SHTypeInfo> type = std::nullopt);
  // Annotates a sub-path of an input variable (e.g. array index or table key)
  // The magic key none is reserved for dynamic keys in table context
  void annotateSubPath(const SHVar &key);
  // Clears the currently tracked variable
  void annotateClearVariable();
  // Annotates that the current shards is pass-through
  void annotatePassthrough();
  // Annotates that the variable input is of a certain type
  void annotateIsOfType(SHTypeInfo);
  // Annotates a negation condition
  void annotateNot();
  // Annotates a negation condition
  void annotateAnd();
  void annotateOr();
  void annotateIsNone();
  void annotateIsNotNone();

  // Temporary
  SHTypeInfo &previousOutputType() { return currentScope().previousOutputType; }

  void step();

  shards::pmr::PolymorphicAllocator<> getAllocator() { return tempAllocator.getAllocator(); }

  static CompositionContext &get(const SHInstanceData &data) {
    shassert(data.privateContext && "Private context should be valid");
    return *reinterpret_cast<CompositionContext *>(data.privateContext);
  }

  compose::FlowAnalysis &current() { return currentScope().flow; }

private:
  void flowTagPassthrough() { current().annotations.isPassthrough = true; }
  void flowTagInstrumented() { current().annotations.isInstrumented = true; }

  // Internal, annotates the next shard to be processed
  void flowAnnotateNextShard(Shard *);
  void flowAnnotateContextVariable(std::string_view name, std::optional<SHTypeInfo> type);
  void flowClearVariable();
  void flowAppendVA(flow::VariableAccessor va);
  void flowClearUndeterministic();
  AssertionRule *flowAnnotateIsA(SHTypeInfo type);
  AssertionRule *flowCommitRule();
};

} // namespace compose
using compose::CompositionContext;
} // namespace shards
struct SHPrivateContext : public shards::CompositionContext {};
#endif /* CDA366C4_E8D0_474B_AFA2_F66229C830BB */
