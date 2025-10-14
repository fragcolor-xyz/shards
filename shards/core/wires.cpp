#include "foundation.hpp"
#include "runtime.hpp"
#include "trait.hpp"
#if SHARDS_DEBUGGER
#include <shards/modules/debugger/interface.hpp>
#endif

using namespace shards;

SHWire::~SHWire() {
  SHLOG_TRACE("Destroying wire {} ({})", name, (void *)this);
  destroy();
}

void SHWire::addTrait(SHTrait inTrait) {
  auto it = std::find_if(traits.begin(), traits.end(), [&](const shards::Trait &t) { return t.sameIdAs(inTrait); });
  if (it == traits.end()) {
    SHLOG_TRACE("Adding <{}> to wire \"{}\"", SHVar{.payload = {.traitValue = &inTrait}, .valueType = SHType::Trait}, name);
    traits.emplace_back(inTrait);

    // Make sure to register the trait
    TraitRegister::instance().insertUnique(inTrait);
  }
}

void SHWire::destroy() {
  for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
    (*it)->cleanup(*it, nullptr);
  }
  for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
    decRef(*it);
  }

  // find dangling variables, notice but do not destroy
  for (auto var : variables) {
    if (var.second.refcount > 0) {
      SHLOG_ERROR("Found a dangling variable: {}, wire: {} (on destroy)", var.first, name);
    }
  }

  if (composeResult) {
    shards::arrayFree(composeResult->requiredInfo);
    shards::arrayFree(composeResult->exposedInfo);
  }

  // finally reset the mesh
  mesh.reset();

#if SH_CORO_NEED_STACK_MEM
  if (stackMem) {
    ::operator delete[](stackMem, std::align_val_t{16});
    stackMem = nullptr;
  }
#endif
}

void SHWire::warmup(SHContext *context) {
  if (!warmedUp) {
    SHLOG_TRACE("Running warmup on wire: {}", name);

    // we likely need this early!
    mesh = context->main->mesh;
    warmedUp = true;

    context->wireStack.push_back(this);
    DEFER({ context->wireStack.pop_back(); });
    for (auto blk : shards) {
      try {
        if (blk->warmup) {
          auto status = blk->warmup(blk, context);
          if (status.code != SH_ERROR_NONE) {
            std::string_view msg(status.message.string, size_t(status.message.len));
            SHLOG_ERROR("Warmup failed on wire: {}, shard: {} ({})", name, blk->name(blk),
                        formatShardSourceLocation(blk));
            throw shards::WarmupError(msg);
          }
        }
        if (context->failed()) {
          throw shards::WarmupError(context->getErrorMessage());
        }
      } catch (const std::exception &e) {
        SHLOG_ERROR("Shard warmup error, failed shard: {}", blk->name(blk));
        SHLOG_ERROR(e.what());
        // if the failure is from an exception context might not be uptodate
        if (!context->failed()) {
          context->cancelFlow(e.what());
        }
        throw;
      } catch (...) {
        SHLOG_ERROR("Shard warmup error, failed shard: {}", blk->name(blk));
        if (!context->failed()) {
          context->cancelFlow("foreign exception failure, check logs");
        }
        throw;
      }
    }

    SHLOG_TRACE("Ran warmup on wire: {}", name);
  } else {
    SHLOG_TRACE("Warmup already run on wire: {}", name);
  }
}

void SHWire::cleanup(bool force) {
  if (force || (warmedUp && wireUsers.size() == 0)) {
    SHLOG_TRACE("Running cleanup on wire: {} users count: {}", name, wireUsers.size());

    warmedUp = false;
    previousOutput = {};

    dispatcher.trigger(SHWire::OnCleanupEvent{this});

    // Run cleanup on all shards, prepare them for a new start if necessary
    // Do this in reverse to allow a safer cleanup
    for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
      auto blk = *it;
      try {
        blk->cleanup(blk, context);
      }
#if SH_BOOST_COROUTINE
      catch (boost::context::detail::forced_unwind const &e) {
        SHLOG_WARNING("Shard cleanup boost forced unwind, failed shard: {}", blk->name(blk));
        throw; // required for Boost Coroutine!
      }
#endif
      catch (const std::exception &e) {
        SHLOG_ERROR("Shard cleanup error, failed shard: {}, error: {}", blk->name(blk), e.what());
      } catch (...) {
        SHLOG_ERROR("Shard cleanup error, failed shard: {}", blk->name(blk));
      }
    }

    // Also clear all variables reporting dangling ones
    for (auto var : variables) {
      if (var.second.refcount > 0) {
        SHLOG_ERROR("Found a dangling variable: {} in wire: {} (on cleanup)", var.first, name);
      }
    }
    variables.clear();

    auto mesh_ = mesh.lock();
    if (mesh_) {
      mesh_->unschedule(shared_from_this());
    }
    mesh.reset();

    resumer = nullptr;

    SHLOG_TRACE("Ran cleanup on wire: {}", name);
  }
}
