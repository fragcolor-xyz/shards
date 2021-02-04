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

#define CHAIN(_name) chainblocks::Chain(_name)
#define GLTF_Load() block("GLTF.Load")
#define GLTF_Load_NoBitangents() block("GLTF.Load", false)
#define Log() block("Log")
#define GFX_MainWindow(_name, _blocks)                                         \
  block("GFX.MainWindow", _name, Var::Any, Var::Any, Blocks()._blocks)
#define Once(_blocks) block("Once", Blocks()._blocks)
#define Set(_name) block("Set", #_name)
#define Ref(_name) block("Ref", #_name)
#define Get(_name) block("Get", #_name)
#define SetTable(_name, _key) block("Set", #_name, #_key)
#define RefTable(_name, _key) block("Ref", #_name, #_key)
#define GFX_Draw(_model) block("GLTF.Draw", Var::ContextVar(#_model))
#define GFX_Draw_WithMaterials(_model, _mats)                                  \
  block("GLTF.Draw", Var::ContextVar(#_model), Var::ContextVar(#_mats))
#define GFX_Camera() block("GFX.Camera")
#define FS_Read_Bytes() block("FS.Read", true)
#define GFX_Shader(_vs, _fs)                                                   \
  block("GFX.Shader", Var::ContextVar(#_vs), Var::ContextVar(#_fs))

namespace chainblocks {
namespace GLTF_Tests {
void testLoad() {
  std::vector<Var> identity = {Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
                               Var(0.0, 0.0, 1.0, 0.0),
                               Var(0.0, 0.0, 0.0, 1.0)};

  const auto vs =
      "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_bump.bin";
  const auto fs =
      "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_bump.bin";

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
                            Once(
                                // load the model
                                let("../deps/tinygltf/models/Cube/Cube.gltf")
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
            .GFX_MainWindow(
                "window",
                Once(
                    // model
                    let("../deps/tinygltf/models/Cube/Cube.gltf")
                        .GLTF_Load_NoBitangents()
                        .Ref(model)
                        .Log()
                        // vertex shader load
                        .let(vs)
                        .FS_Read_Bytes()
                        .Ref(vs_bytes)
                        // fragment shader load
                        .let(fs)
                        .FS_Read_Bytes()
                        .Ref(fs_bytes)
                        // setup shader program
                        .GFX_Shader(vs_bytes, fs_bytes)
                        // setup material override table
                        .RefTable(mat1, Shader)
                        .Get(mat1)
                        .RefTable(mats,
                                  Cube) // Cube is the material name in the json
                    )
                    .let(0.0, 0.0, 10.0)
                    .RefTable(cam, Position)
                    .let(0.0, 0.0, 0.0)
                    .RefTable(cam, Target)
                    .Get(cam)
                    .GFX_Camera()
                    .let(identity)
                    .GFX_Draw_WithMaterials(model, mats));
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