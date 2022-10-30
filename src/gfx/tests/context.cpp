
#include "./context.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/utils.hpp>
#include <spdlog/fmt/fmt.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

static std::string currentTestName = "<unknown>";
static std::string currentTestTags = "";

class TestRunListener : public Catch::EventListenerBase {
public:
  using Catch::EventListenerBase::EventListenerBase;
  void testCaseStarting(Catch::TestCaseInfo const &testInfo) override {
    currentTestName = testInfo.name;
    currentTestTags = testInfo.tagsAsString();
  }
};

CATCH_REGISTER_LISTENER(TestRunListener);

namespace gfx {

bool isTestDebuggingEnabled() {
  static bool enabled = []() { return getEnvFlag("GFX_TESTS_DEBUG"); }();
  return enabled;
}

TestContext::TestContext(const ContextCreationOptions &options, const int2 &resolution) {
  if (isTestDebuggingEnabled()) {
    window = std::make_shared<Window>();
    window->init(WindowCreationOptions{
        .width = resolution.x,
        .height = resolution.y,
        .title = fmt::format("TEST: {} {}", currentTestName, currentTestTags),
    });
    Context::init(*window.get(), options);
  } else {
    Context::init(options);
  }

  waitUntilReady();
}

void TestContext::waitUntilReady() {
  while (!Context::isReady()) {
#ifdef __EMSCRIPTEN__
    emscripten_sleep(1);
#endif
    Context::tickRequesting();
  }
}

bool TestContext::tick() {
  if (window) {
    std::vector<SDL_Event> events{};
    window->pollEvents(events);
    for (auto &event : events) {
      if (event.type == SDL_QUIT)
        return false;
    }
  }
  return true;
}
} // namespace gfx
