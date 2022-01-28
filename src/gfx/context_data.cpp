#include "context_data.hpp"
#include "context.hpp"

namespace gfx {
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

void ContextData::releaseConditional() {
	if (context) {
		release();
		unbindFromContext();
	}
}

} // namespace gfx
