#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>

int main(int argc, char *argv[]) {
  Catch::Session session;

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0) // Indicates a command line error
    return returnCode;

  auto &configData = session.configData();

  int result = session.run();

  return result;
}