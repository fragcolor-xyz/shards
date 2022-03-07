#include "context_data.hpp"
#include "context.hpp"

namespace gfx {
void WithContextData::bindToContext(Context *context) {
  assert(!this->context);
  assert(context);
  this->context = context;
  this->context->addContextDataObjectInternal(weak_from_this());
}

void WithContextData::unbindFromContext() {
  assert(context);
  context->removeContextDataObjectInternal(this);
  context = nullptr;
}
} // namespace gfx
