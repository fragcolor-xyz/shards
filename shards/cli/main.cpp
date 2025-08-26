/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/modules/langffi/bindings.h>
#include <shards/core/utils.hpp>
#include <boost/filesystem.hpp>

int main(int argc, const char *argv[]) {
  using namespace shards::literals;
  shards::pushThreadName("Main Thread"_ns);
  shards::parseArguments(argc, argv);

  // Functionality is defined in shards-lang rust crate
  auto result = shards_process_args(argc, const_cast<char **>(argv), false);
  return result;
}