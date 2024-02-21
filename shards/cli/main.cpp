/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/lang/bindings.h>
#include <shards/core/utils.hpp>
#include <shards/core/foundation.hpp>
#include <boost/filesystem.hpp>

int main(int argc, const char *argv[]) {
  shards::parseArguments(argc, argv);
  shards::pushThreadName("Main Thread");

  // Functionality is defined in shards-lang rust crate
  auto result = shards_process_args(argc, const_cast<char **>(argv), false);
  return result;
}