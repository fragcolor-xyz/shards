/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include <random>

#include "../../include/ops.hpp"
#include "../../include/utility.hpp"
#include "../core/runtime.hpp"

#undef CHECK

#define CATCH_CONFIG_RUNNER

#include <catch2/catch_all.hpp>

int main(int argc, char *argv[]) {
	chainblocks::GetGlobals().RootPath = "./";
	registerCoreBlocks();
	int result = Catch::Session().run(argc, argv);
#ifdef __EMSCRIPTEN_PTHREADS__
	// in this case we need to call exit our self
	exit(0);
#endif
	return result;
}

namespace chainblocks {}

using namespace chainblocks;
