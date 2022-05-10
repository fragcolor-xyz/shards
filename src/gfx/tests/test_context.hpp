#ifndef GFX_TESTS_TEST_CONTEXT
#define GFX_TESTS_TEST_CONTEXT

#include <gfx/context.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace gfx {
// Creates and initializes a test context synchronously
inline std::shared_ptr<Context> createTestContext(const ContextCreationOptions &options = ContextCreationOptions()) {
  auto context = std::make_shared<Context>();
  context->init(options);
  while (!context->isReady()) {
#ifdef __EMSCRIPTEN__
    emscripten_sleep(1);
#endif
    context->tickRequesting();
  }
  return context;
}
} // namespace gfx

#endif // GFX_TESTS_TEST_CONTEXT
