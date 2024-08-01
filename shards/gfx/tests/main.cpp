#include <SDL3/SDL_timer.h>
#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <spdlog/spdlog.h>
#include <shards/log/log.hpp>

// Defined in the gfx rust crate
//   used to initialize tracy on the rust side, since it required special intialization (C++ doesn't)
//   but since we link to the dll, we can use it from C++ too
extern "C" void gfxTracyInit();

int main(int argc, char *argv[]) {
#ifdef TRACY_ENABLE
  gfxTracyInit();
#endif 
  shards::logging::setupDefaultLoggerConditional("test-gfx.log");

  Catch::Session session;

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0) // Indicates a command line error
    return returnCode;

  auto &configData = session.configData();
  (void)configData;

  int result = session.run();

#ifdef TRACY_ENABLE
  SDL_Delay(1000);
#endif

  return result;
}
