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

namespace compose {
extern std::shared_ptr<spdlog::logger> logger;

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

struct Variable {
  size_t id;
  SHExposedTypeInfo type;
};

struct ComposedWire;
struct Scope {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;
  Scope(std::allocator_arg_t, allocator_type a);
  Scope(std::allocator_arg_t, allocator_type a, Scope &&other);

  size_t id;

  FlowAnalysis flow;

  struct PerShard {};
  pmr::vector<PerShard> shards;
  pmr::unordered_map<std::string_view, SHExposedTypeInfo> exposed;
  pmr::unordered_set<SHExposedTypeInfo> required;
  std::unordered_map<std::string_view, SHExposedTypeInfo> *fullRequired{nullptr};
  pmr::unordered_map<std::string_view, size_t> variableMap;

  SHTypeInfo previousOutputType{};
  SHTypeInfo originalInputType{};

  Shard *bottom{};
  Shard *next{};
  std::shared_ptr<ComposedWire> wire;

  bool onWorkerThread{false};

  std::string_view wireName() const;
};

struct ComposedWire {
  std::vector<Variable> variables;
  // Span [0, numExtVariables) contains externally added variables
  size_t numExtVariables{};
  // Span [numExtVariables, variables.size()) contains global variables
  size_t numGlobalVariables{};
  std::unordered_map<std::string_view, SHExternalVariable> required;
  SHWire *source;

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

  size_t idAllocator{};

  CompositionContext();
  ~CompositionContext();

  Variable *findVariable(std::string_view name);
  Variable *insertVariable(std::string_view name, SHExposedTypeInfo type);

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

  Scope &currentScope() { return *stack.back(); }
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
