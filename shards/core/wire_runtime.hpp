#ifndef D37B8DAE_36C5_4EDA_B75D_70062AC0E0F9
#define D37B8DAE_36C5_4EDA_B75D_70062AC0E0F9

#include "foundation.hpp"
#include <boost/core/span.hpp>
#include <vector>

namespace shards {

struct WireVariableSlot {
  // This points into a wire reference slot
  SHVar **ptr;

  WireVariableSlot(SHVar **ptr) : ptr(ptr) {}

  // Checks if the reference slot it valid ONLY!
  //  to check if the reference is actually assigned to something, check isAssigned()
  bool isValid() const { return ptr != nullptr; }

  // Checks if the reference is actually assigned to something
  bool isAssigned() const { return isValid() && *ptr != nullptr; }

  SHVar &operator*() const {
    shassert(isAssigned());
    return **ptr;
  }

  SHVar *operator->() const {
    shassert(isAssigned());
    return *ptr;
  }

  SHVar *get() const {
    shassert(isValid());
    return *ptr;
  }
};

struct WireRuntimeVariableInfo {

  // Pointers to variables
  // for local variables, this will point into variableStorage
  // for refs/externals/inherited/global, these will be initialized to point to the correct references during warmup
  // SAME STRICT ORDERING AS variables
  // locals -> refs -> inherited -> external -> global
  std::vector<SHVar *> variableSlots;

  // Local variable storage
  std::vector<SHVar> variableStorage;

  struct VariableDecl {
    std::string name;
    TypeInfo type;
  };

  // STRICT ORDERING
  // locals -> refs -> inherited -> external -> global
  std::vector<VariableDecl> variables;

  uint32_t numLocalVariables{};
  uint32_t numRefVariables{};
  uint32_t numInheritedVariables{};
  uint32_t numExternalVariables{};
  uint32_t numGlobalVariables{};

  struct VariableScope {
    std::unordered_map<std::string, size_t> variableLookup;
  };

  // Maps shard sequence id to variable a given variable scope
  // sorted for bounds search
  std::map<size_t, VariableScope> variableScopes;

  uint32_t localOffset() const { return 0; }
  uint32_t refOffset() const { return numLocalVariables; }
  uint32_t inheritedOffset() const { return numLocalVariables + numRefVariables; }
  uint32_t externalOffset() const { return numLocalVariables + numRefVariables + numInheritedVariables; }
  uint32_t globalOffset() const { return numLocalVariables + numRefVariables + numInheritedVariables + numExternalVariables; }

  boost::span<const VariableDecl> localVariables() const { //
    return boost::span(variables).subspan(localOffset(), numLocalVariables);
  }
  boost::span<const VariableDecl> refVariables() const { //
    return boost::span(variables).subspan(refOffset(), numRefVariables);
  }
  boost::span<const VariableDecl> inheritedVariables() const {
    return boost::span(variables).subspan(inheritedOffset(), numInheritedVariables);
  }
  boost::span<const VariableDecl> externalVariables() const {
    return boost::span(variables).subspan(externalOffset(), numExternalVariables);
  }
  boost::span<const VariableDecl> globalVariables() const {
    return boost::span(variables).subspan(globalOffset(), numGlobalVariables);
  }

  size_t subspanOffset(boost::span<const VariableDecl> span) const { return std::distance(variables.data(), &span[0]); }

  WireRuntimeVariableInfo() = default;
  WireRuntimeVariableInfo(const WireRuntimeVariableInfo &) = delete;
  WireRuntimeVariableInfo &operator=(const WireRuntimeVariableInfo &) = delete;
  ~WireRuntimeVariableInfo() { cleanupStorage(); }
  void cleanupStorage() {}

  void initStorage() {
    variableSlots.resize(variables.size());
    variableStorage.resize(numLocalVariables);
    for (auto &v : variableStorage) {
      // Make them ref-counted
      v.flags = SHVAR_FLAGS_REF_COUNTED;
      v.refcount = 0;
    }

    // Populate reference sotrage
    size_t localOffset_ = localOffset();
    for (size_t i = 0; i < numLocalVariables; ++i) {
      variableSlots[i + localOffset_] = &variableStorage[i];
    }
  }

  WireVariableSlot variableFromId(size_t vid) {
    shassert(vid < variables.size() && "Invalid local variable id");
    return &variableSlots[vid];
  }

  WireVariableSlot findReferenceStrict(Shard *shard, std::string_view name) {
    auto it = variableScopes.find(shard->seqId);
    if (it == variableScopes.end())
      return nullptr;
    auto search = it->second.variableLookup.find(std::string(name)); // TODO: optimize
    if (search == it->second.variableLookup.end())
      return nullptr;
    return variableFromId(search->second);
  }

  size_t slotIndex(SHVar *const *slot) const { return std::distance(variableSlots.data(), slot); }

  std::string slotDebugName(SHVar **slot) const {
    shassert(slot);
    return slotDebugName(slotIndex(slot));
  }
  std::string slotDebugName(size_t slot) const {
    if (slot < refOffset()) {
      return fmt::format("local: {} [{}]", variables[slot].name, slot);
    } else if (slot < inheritedOffset()) {
      return fmt::format("ref: {} [{}]", variables[slot].name, slot);
    } else if (slot < externalOffset()) {
      return fmt::format("inherited: {} [{}]", variables[slot].name, slot);
    } else if (slot < globalOffset()) {
      return fmt::format("external: {} [{}]", variables[slot].name, slot);
    } else if (slot < variables.size()) {
      return fmt::format("global: {} [{}]", variables[slot].name, slot);
    } else {
      return fmt::format("invalid: [{}]", slot);
    }
  }

  WireVariableSlot findReference(Shard *shard, std::string_view name) {
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
      auto search = scope.variableLookup.find(std::string(name)); // TODO: optimize
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
} // namespace shards

#endif /* D37B8DAE_36C5_4EDA_B75D_70062AC0E0F9 */
