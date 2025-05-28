#include "compose.hpp"
#include "runtime.hpp"
#include "pmr/unordered_set.hpp"
#include "pmr/vector.hpp"
#include "pmr/string.hpp"
#include "type_cache.hpp"
#include <shards/common_types.hpp>
#include <tracy/Wrapper.hpp>
#include <shards/log/log.hpp>

namespace shards {

SHVar *referenceWireVariable(SHWire *wire, std::string_view name) {
  // SHVar &v = wire->getVariable(toSWL(name));
  // v.refcount++;
  // v.flags |= SHVAR_FLAGS_REF_COUNTED;
  // return &v;
  // auto wire = ctx->wireStack.back();
  // shassert(wire->runtimeVariableInfo);
  // auto v = wire->runtimeVariableInfo->findReferenceStrict(ctx->internal.currentShard, name);
  // if (v) {
  //   if (v->flags & SHVAR_FLAGS_REF_COUNTED)
  //     v->refcount++;
  //   else {
  //     shassert(v->flags & SHVAR_FLAGS_EXTERNAL);
  //   }
  //   return v;
  // }
  throw std::logic_error("TODO");
}

SHVar *referenceWireVariable(SHWireRef wire, std::string_view name) {
  auto swire = SHWire::sharedFromRef(wire);
  return referenceWireVariable(swire.get(), name);
}

SHVar *referenceGlobalVariable(SHContext *ctx, std::string_view name) {
  auto mesh = ctx->main->mesh.lock();
  shassert(mesh);

  SHVar &v = mesh->getVariable(toSWL(name));
  v.refcount++;
  if (v.refcount == 1) {
    SHLOG_TRACE("Creating a global variable, wire: {} name: {}", ctx->wireStack.back()->name, name);
  }
  v.flags |= SHVAR_FLAGS_REF_COUNTED;
  return &v;
}

static SHVar *findVariableInternal(SHContext *ctx, std::string_view name, bool allowReferences = false) {
  // try find a wire variable
  // from top to bottom of wire stack

  auto wire = ctx->wireStack.back();
  shassert(wire->runtimeVariableInfo);
  auto v = wire->runtimeVariableInfo->findReferenceStrict(ctx->internal.currentShard, name);
  if (v.isValid()) {
    if (!v.isAssigned()) {
      throw std::logic_error(fmt::format("Variable slot {} is not assigned", wire->runtimeVariableInfo->slotDebugName(v.ptr)));
    }
    if (v->flags & SHVAR_FLAGS_REF_COUNTED)
      v->refcount++;
    else {
      shassert(v->flags & SHVAR_FLAGS_EXTERNAL);
    }
    return v.get();
  }

  //   auto rit = ctx->wireStack.rbegin();
  //   bool first = true;
  //   for (; rit != ctx->wireStack.rend(); ++rit, first = false) {
  //     auto wire = *rit;
  //     shassert(wire->runtimeVariableInfo);
  //     auto v = first ? wire->runtimeVariableInfo->findReferenceStrict(ctx->internal.currentShard, name)
  //                    : wire->runtimeVariableInfo->findReference(ctx->internal.currentShard, name);
  //     if (v) {
  //       if (v->flags & SHVAR_FLAGS_REF_COUNTED)
  //         v->refcount++;
  //       else {
  //         shassert(v->flags & SHVAR_FLAGS_EXTERNAL);
  //       }
  //       return v;
  //     }
  //     /*
  //     // prioritize local variables
  //     auto ov = wire->getVariableIfExists(toSWL(name));
  //     if (ov) {
  //       // found, lets get out here
  //       SHVar &cv = (*ov).get();
  //       cv.refcount++;
  //       cv.flags |= SHVAR_FLAGS_REF_COUNTED;
  //       return &cv;
  //     }
  //     // try external variables
  //     auto ev = wire->getExternalVariableIfExists(toSWL(name));
  //     if (ev) {
  //       // found, lets get out here
  //       SHVar &cv = *ev;
  //       shassert((cv.flags & SHVAR_FLAGS_EXTERNAL) != 0);
  //       return &cv;
  //     }
  //     */
  //     // if this wire is pure we break here and do not look further
  //     if (wire->pure) {
  //       break; // exit early, continue with mesh lookup
  //     }
  //   }
  // }

  // try using mesh
  {
    auto mesh = ctx->main->mesh.lock();
    shassert(mesh);

    // Was not in wires.. find in mesh
    {
      auto ov = mesh->getVariableIfExists(toSWL(name));
      if (ov) {
        // found, lets get out here
        SHVar &cv = (*ov).get();
        cv.refcount++;
        cv.flags |= SHVAR_FLAGS_REF_COUNTED;
        return &cv;
      }
    }

    // Was not in mesh directly.. try find in meshs refs
    {
      auto rv = mesh->getRefIfExists(toSWL(name));
      if (rv) {
        SHLOG_TRACE("Referencing a parent node variable, wire: {} name: {}", ctx->wireStack.back()->name, name);
        // found, lets get out here
        rv->refcount++;
        rv->flags |= SHVAR_FLAGS_REF_COUNTED;
        return rv;
      }
    }
  }

  return nullptr;
}

SHVar *findVariable(SHContext *ctx, std::string_view name) { return findVariableInternal(ctx, name); }

#define SH_DEBUG_UNFOUND_VARIABLES 1

SHVar *referenceVariable(SHContext *ctx, std::string_view name) {
  SHVar *var = findVariable(ctx, name);
  if (var)
    return var;

  auto shard = ctx->internal.currentShard;

#if SH_DEBUG_UNFOUND_VARIABLES
  SHLOG_ERROR("Variable not found: {}", name);
  var = findVariable(ctx, name);
#endif

  throw std::logic_error(
      fmt::format("Variable not found {}, shard: {} (Id: {}, SId: {})", name, shard->name(shard), shard->id, shard->seqId));

  // shassert(false);

  // // worst case create in current top wire!
  // SHLOG_TRACE("Creating a variable, wire: {} name: {}", ctx->wireStack.back()->name, name);
  // SHVar &cv = ctx->wireStack.back()->getVariable(toSWL(name));
  // shassert(cv.refcount == 0);
  // cv.refcount++;
  // // can safely set this here, as we are creating a new variable
  // cv.flags = SHVAR_FLAGS_REF_COUNTED;
  // return &cv;
}

void releaseVariableRef(SHVar *&variable) {
  if (variable) {
    variableReleaseReference(variable);
    variable = nullptr;
  }
}

void releaseVariable(SHVar *variable) {
  if (!variable)
    return;
  variableReleaseReference(variable);
}

SHVar **referenceVariableSlot(SHContext *ctx, std::string_view name) {
  auto wire = ctx->wireStack.back();
  shassert(wire->runtimeVariableInfo);
  auto v = wire->runtimeVariableInfo->findReferenceStrict(ctx->internal.currentShard, name);
  if (!v.isValid())
    throw std::logic_error(fmt::format("Variable slot not found: {}, on shard {} (line: {}, col: {})", name,
                                       ctx->internal.currentShard->name(ctx->internal.currentShard),
                                       ctx->internal.currentShard->line, ctx->internal.currentShard->column));
  return v.ptr;
}

void releaseVariableSlot(SHVar **slot) {
  // if (slot) {
  //   releaseVariable(*slot);
  // }
}

void variableAddReference(SHVar *v) {
  shassert(v);
  if (v->flags & SHVAR_FLAGS_REF_COUNTED)
    v->refcount++;
}

void variableReleaseReference(SHVar *v) {
  shassert(v);
  if ((v->flags & SHVAR_FLAGS_REF_COUNTED) != 0) {
    shassert((v->flags & SHVAR_FLAGS_REF_COUNTED) == SHVAR_FLAGS_REF_COUNTED && "Variable is not ref counted");
    shassert(v->refcount > 0 && "Variable ref count is 0");
    v->refcount--;
    if (v->refcount == 0) {
      SHLOG_TRACE("Destroying a variable (0 ref count), type: {}", type2Name(v->valueType));
      destroyVar(*v);
    }
  }
}

} // namespace shards
