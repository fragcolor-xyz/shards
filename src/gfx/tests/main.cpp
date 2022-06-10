#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {
  Catch::Session session;

  spdlog::set_level(spdlog::level::debug);

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0) // Indicates a command line error
    return returnCode;

  auto &configData = session.configData();
  (void)configData;

  int result = session.run();

  return result;
}
