#include "context_data_storage.hpp"
#include "../core/assert.hpp"

namespace gfx {

// For now this uses a global mutex
// We can get rid of this once ContextData is moved to context/renderer storage
// std::recursive_mutex ContextData::globalMutex;

// void ContextData::bindToContext(Context &context) {
//   shassert(!this->context);
//   this->context = &context;
//   this->context->addContextDataInternal(weak_from_this());
// }

// void ContextData::unbindFromContext() {
//   shassert(context);
//   context->removeContextDataInternal(this);
//   context = nullptr;
// }

// void ContextData::releaseContextDataConditional() {
//   ContextData::globalMutex.lock();

//   if (context) {
//     releaseContextData();
//     unbindFromContext();
//   }

//   ContextData::globalMutex.unlock();
// }

} // namespace gfx
