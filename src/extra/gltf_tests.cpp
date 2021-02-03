/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

// included in gltf.cpp

#ifndef CB_INTERNAL_TESTS
// just for linting purposes
#include "./gltf.cpp"
#endif

#ifdef CHECK
#undef CHECK
#endif

#if defined(_WIN32)
#define SHADERS_FOLDER "dx11"
#elif defined(__APPLE__)
#define SHADERS_FOLDER "metal"
#else
#define SHADERS_FOLDER "glsl"
#endif

#include <catch2/catch_all.hpp>

#define CHAIN(name) chainblocks::Chain(name)
#define Const(val) block("Const", val)
#define GLTF_Load block("GLTF.Load")
#define Log block("Log")
#define GFX_MainWindow(name, blocks)                                           \
  block("GFX.MainWindow", name, Var::Any, Var::Any, Blocks().blocks)

namespace chainblocks {
namespace GLTF_Tests {
void testLoad() {
  SECTION("Fail-Not-Existing") {
    auto chain =
        CHAIN("test-chain")
            .GFX_MainWindow("window", Const("../Cube.gltf").GLTF_Load.Log);
    auto node = CBNode::make();
    node->schedule(chain);
    while (true) {
      if (!node->tick())
        break;
    }
    auto errors = node->errors();
    REQUIRE(errors[0] == "GLTF model file does not exist.");
  }

  SECTION("Cube1-Text") {
    auto chain =
        CHAIN("test-chain")
            .GFX_MainWindow(
                "window",
                Const("../deps/tinygltf/models/Cube/Cube.gltf").GLTF_Load.Log);
    auto node = CBNode::make();
    node->schedule(chain);
    while (true) {
      REQUIRE(node->tick());
      if (node->empty())
        break;
      sleep(0.1);
    }
    auto errors = node->errors();
    REQUIRE(errors.size() == 0);
  }

  SECTION("Cube2-Text") {
    auto chain =
        CHAIN("test-chain")
            .GFX_MainWindow(
                "window",
                Const("../external/glTF-Sample-Models/2.0/Box/glTF/Box.gltf")
                    .GLTF_Load.Log);
    auto node = CBNode::make();
    node->schedule(chain);
    while (true) {
      REQUIRE(node->tick());
      if (node->empty())
        break;
      sleep(0.1);
    }
    auto errors = node->errors();
    REQUIRE(errors.size() == 0);
  }
}
} // namespace GLTF_Tests
} // namespace chainblocks