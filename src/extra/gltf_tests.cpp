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
#define GLTF_Load() block("GLTF.Load")
#define GLTF_Load_NoBitangents() block("GLTF.Load", false)
#define Log() block("Log")
#define GFX_MainWindow(name, blocks)                                           \
  block("GFX.MainWindow", name, Var::Any, Var::Any, Blocks().blocks)
#define Once(blocks) block("Once", Blocks().blocks)
#define Set(name) block("Set", #name)
#define Ref(name) block("Ref", #name)
#define Get(name) block("Get", #name)
#define SetTable(name, key) block("Set", #name, #key)
#define RefTable(name, key) block("Ref", #name, #key)
#define GFX_Draw(model) block("GLTF.Draw", Var::ContextVar(#model))
#define GFX_Camera() block("GFX.Camera")

namespace chainblocks {
namespace GLTF_Tests {
void testLoad() {
  std::vector<Var> identity = {Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
                               Var(0.0, 0.0, 1.0, 0.0),
                               Var(0.0, 0.0, 0.0, 1.0)};

  SECTION("Fail-Not-Existing") {
    auto chain =
        CHAIN("test-chain")
            .GFX_MainWindow("window", let("../Cube.gltf").GLTF_Load().Log());
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
            .looped(true)
            .GFX_MainWindow("window",
                            Once(let("../deps/tinygltf/models/Cube/Cube.gltf")
                                     .GLTF_Load()
                                     .Ref(model)
                                     .Log())
                                .let(0.0, 0.0, 10.0)
                                .RefTable(cam, Position)
                                .let(0.0, 0.0, 0.0)
                                .RefTable(cam, Target)
                                .Get(cam)
                                .GFX_Camera()
                                .let(identity)
                                .GFX_Draw(model));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 50;
    while (count--) {
      REQUIRE(node->tick());
      if (node->empty())
        break;
      sleep(0.1);
    }
    auto errors = node->errors();
    REQUIRE(errors.size() == 0);
  }

  SECTION("Cube1-Text-NoBitangents") {
    auto chain =
        CHAIN("test-chain")
            .looped(true)
            .GFX_MainWindow("window",
                            Once(let("../deps/tinygltf/models/Cube/Cube.gltf")
                                     .GLTF_Load_NoBitangents()
                                     .Ref(model)
                                     .Log())
                                .let(0.0, 0.0, 10.0)
                                .RefTable(cam, Position)
                                .let(0.0, 0.0, 0.0)
                                .RefTable(cam, Target)
                                .Get(cam)
                                .GFX_Camera()
                                .let(identity)
                                .GFX_Draw(model));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 50;
    while (count--) {
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
                let("../external/glTF-Sample-Models/2.0/Box/glTF/Box.gltf")
                    .GLTF_Load()
                    .Log());
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