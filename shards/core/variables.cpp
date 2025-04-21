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
  SHVar &v = wire->getVariable(toSWL(name));
  v.refcount++;
  v.flags |= SHVAR_FLAGS_REF_COUNTED;
  return &v;
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

SHVar *findVariable(SHContext *ctx, std::string_view name) {
  // try find a wire variable
  // from top to bottom of wire stack

  auto wire = ctx->wireStack.back();
  shassert(wire->runtimeVariableInfo);
  auto v = wire->runtimeVariableInfo->findReferenceStrict(ctx->internal.currentShard, name);
  if (v) {
    if (v->flags & SHVAR_FLAGS_REF_COUNTED)
      v->refcount++;
    else {
      shassert(v->flags & SHVAR_FLAGS_EXTERNAL);
    }
    return v;
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

SHVar *referenceVariable(SHContext *ctx, std::string_view name) {
  SHVar *var = findVariable(ctx, name);
  if (var)
    return var;

  auto shard = ctx->internal.currentShard;
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

static ALWAYS_INLINE void releaseVariableNoCheck(SHVar *variable) {
  if ((variable->flags & SHVAR_FLAGS_EXTERNAL) != 0) {
    return;
  }

  shassert((variable->flags & SHVAR_FLAGS_REF_COUNTED) == SHVAR_FLAGS_REF_COUNTED && "Variable is not ref counted");
  shassert(variable->refcount > 0 && "Variable ref count is 0");

  variable->refcount--;
  if (variable->refcount == 0) {
    SHLOG_TRACE("Destroying a variable (0 ref count), type: {}", type2Name(variable->valueType));
    destroyVar(*variable);
  }
}

void releaseVariableRef(SHVar *&variable) {
  if (variable) {
    releaseVariableNoCheck(variable);
    variable = nullptr;
  }
}

void releaseVariable(SHVar *variable) {
  if (!variable)
    return;
  releaseVariableNoCheck(variable);
}
} // namespace shards
