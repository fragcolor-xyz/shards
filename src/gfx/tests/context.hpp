#ifndef GFX_TESTS_CONTEXT
#define GFX_TESTS_CONTEXT

#include <gfx/context.hpp>
#include <gfx/window.hpp>
#include <memory>

namespace gfx {

// Enabling debugging creates a window so the test can be debugging using RenderDoc/interactive
bool isTestDebuggingEnabled();

struct TestContext : public Context {
  std::shared_ptr<Window> window;

  TestContext(const ContextCreationOptions &options, const int2 &resolution);
  void waitUntilReady();
  bool tick();
};

// Creates and initializes a test context synchronously
inline std::shared_ptr<TestContext> createTestContext(const ContextCreationOptions &options = ContextCreationOptions(),
                                                      const int2 &resolution = int2(1280, 720)) {
  return std::make_shared<TestContext>(options, resolution);
}
} // namespace gfx

#endif // GFX_TESTS_CONTEXT
