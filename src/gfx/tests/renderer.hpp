#ifndef GFX_TESTS_RENDERER
#define GFX_TESTS_RENDERER

#include "../utils.hpp"
#include "./context.hpp"
#include "./data.hpp"
#include <gfx/context.hpp>
#include <gfx/renderer.hpp>

namespace gfx {

// Switches between headless/window renderer
// default is headless
// if GFX_TESTS_DEBUG is set, will render inside a window loop
struct TestRenderer {
  std::shared_ptr<TestContext> context;
  std::shared_ptr<Renderer> renderer;

  WGPUTexture rtTexture{};
  WGPUTextureFormat rtFormat{};
  WGPUTextureView rtView{};
  int2 rtSize{};

  bool debugging{};

  TestRenderer(std::shared_ptr<TestContext> context);
  ~TestRenderer();

  void createRenderTarget(int2 res = int2(1280, 720));
  void cleanupRenderTarget();

  TestFrame getTestFrame();

  TestData getTestData() { return TestData(TestPlatformId::get(*context.get())); }
  bool checkFrame(const char *key, float tolerance = 0.0f) { return getTestData().checkFrame(key, getTestFrame(), tolerance); }

  inline void begin() {
    for (size_t i = 0; !context->beginFrame(); i++) {
      if (i >= 16)
        throw std::runtime_error("Failed to start rendering");
    }
    renderer->beginFrame();
  }

  inline bool end() {
    renderer->endFrame();
    context->endFrame();
    return !debugging;
  }
};

struct _I {
  TestRenderer &renderer;
  _I(std::shared_ptr<TestRenderer> &renderer) : renderer(*renderer.get()) {}
  _I &operator<<(const std::function<void()> _body) {
    while (renderer.context->tick()) {
      renderer.begin();
      _body();
      if (renderer.end())
        break;
    }
    return *this;
  }
};

// Creats the default test renderer with render target setup
inline std::shared_ptr<TestRenderer> createTestRenderer(const int2 &resolution = int2(1280, 720)) {
  auto testRenderer = std::make_shared<TestRenderer>(createTestContext(ContextCreationOptions{}, resolution));
  testRenderer->createRenderTarget(resolution);
  return testRenderer;
}

inline TestData getTestData(std::shared_ptr<TestRenderer> testRenderer) {
  return TestData(TestPlatformId::get(*testRenderer->context.get()));
}

#define TEST_RENDER_LOOP(_testRenderer) _I(_testRenderer) << [&]()
} // namespace gfx

#endif // GFX_TESTS_RENDERER
