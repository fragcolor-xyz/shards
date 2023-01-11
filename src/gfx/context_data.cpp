#include "context_data.hpp"
#include "context.hpp"

namespace gfx {

// For now this uses a global mutex
// We can get rid of this once ContextData is moved to context/renderer storage
std::recursive_mutex ContextData::globalMutex;

void ContextData::bindToContext(Context &context) {
  assert(!this->context);
  this->context = &context;
  this->context->addContextDataInternal(weak_from_this());
}

void ContextData::unbindFromContext() {
  assert(context);
  context->removeContextDataInternal(this);
  context = nullptr;
}

void ContextData::releaseContextDataConditional() {
  ContextData::globalMutex.lock();

  if (context) {
    releaseContextData();
    unbindFromContext();
  }

  ContextData::globalMutex.unlock();
}

} // namespace gfx
