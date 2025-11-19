/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/modules/langffi/bindings.h>
#include <shards/core/utils.hpp>
#include <boost/filesystem.hpp>

#if SHARDS_DEBUGGER
#include <shards/modules/debugger/interface.hpp>
#endif

int main(int argc, const char *argv[]) {
  using namespace shards::literals;
  shards::pushThreadName("Main Thread"_ns);
  shards::parseArguments(argc, argv);

  // Functionality is defined in shards-lang rust crate
  auto result = shards_process_args(argc, const_cast<char **>(argv), false);

#if SHARDS_DEBUGGER
  shards::dbg::unload();
#endif
  return result;
}