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

#if defined(BGFX_CONFIG_RENDERER_OPENGL) || defined(__linux__) ||              \
    defined(__EMSCRIPTEN__)
#define SHADERS_FOLDER "glsl"
#elif defined(__APPLE__)
#define SHADERS_FOLDER "metal"
#elif defined(_WIN32)
#define SHADERS_FOLDER "dx11"
#endif

#include <catch2/catch_all.hpp>

#include <chain_dsl.hpp>

namespace chainblocks {
namespace GLTF_Tests {
std::vector<Var> identity = {Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
                             Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

std::vector<Var> scale10 = {Var(10.0, 0.0, 0.0, 0.0), Var(0.0, 10.0, 0.0, 0.0),
                            Var(0.0, 0.0, 10.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

const auto vs =
    "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_bump.bin";
const auto fs =
    "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_bump.bin";

void testGLTFModel(CBString name, CBString modelPath, CBString colorPath,
                   CBString normalPath, CBString matName, Var camera_pos) {
  SECTION(name) {
    DefChain(test_chain)
        .Looped()
        .GFX_MainWindow("window",
                        Once(let(modelPath)
                                 .GLTF_Load_NoShaders() Ref(model)
                                 .Log()
                                 .LoadImage(colorPath)
                                 .GFX_Texture2D() PushTable(mat1, "Textures")
                                 .LoadImage(normalPath)
                                 .GFX_Texture2D() PushTable(mat1, "Textures")
                                 .let(vs)
                                 .FS_Read_Bytes() Ref(vs_bytes)
                                 .let(fs)
                                 .FS_Read_Bytes() Ref(fs_bytes)
                                 .GFX_Shader(vs_bytes, fs_bytes)
                                     SetTable(mat1, "Shader")
                                 .Get(mat1) SetTable(mats, matName))
                            .let(camera_pos) SetTable(cam, "Position")
                            .let(0.0, 0.0, 0.0) SetTable(cam, "Target")
                            .Get(cam)
                            .GFX_Camera()
                            .let(identity)
                            .GLTF_Draw_WithMaterials(model, mats));
    auto node = CBNode::make();
    node->schedule(test_chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick());
      if (node->empty())
        break;
      sleep(0.1);
    }
    auto errors = node->errors();
    REQUIRE(errors.size() == 0);
  }
}

void testGLTFModel(CBString name, CBString modelPath, CBString matName,
                   Var camera_pos) {
  SECTION(name) {
    DefChain(test_chain)
        .Looped()
        .GFX_MainWindow("window", Once(let(modelPath)
                                           .GLTF_Load_NoShaders() Ref(model)
                                           .Log()
                                           .let(vs)
                                           .FS_Read_Bytes() Ref(vs_bytes)
                                           .let(fs)
                                           .FS_Read_Bytes() Ref(fs_bytes)
                                           .GFX_Shader(vs_bytes, fs_bytes)
                                               SetTable(mat1, "Shader")
                                           .Get(mat1) SetTable(mats, matName))
                                      .let(camera_pos) SetTable(cam, "Position")
                                      .let(0.0, 0.0, 0.0)
                                          SetTable(cam, "Target")
                                      .Get(cam)
                                      .GFX_Camera()
                                      .let(identity)
                                      .GLTF_Draw_WithMaterials(model, mats));
    auto node = CBNode::make();
    node->schedule(test_chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick());
      if (node->empty())
        break;
      sleep(0.1);
    }
    auto errors = node->errors();
    REQUIRE(errors.size() == 0);
  }
}

void testGLTFModel(CBString name, CBString modelPath, Var camera_pos,
                   const std::vector<Var> &btransform = identity,
                   const std::vector<Var> &atransform = identity) {
  SECTION(name) {
    DefChain(test_chain)
        .Looped()
        .GFX_MainWindow(
            "window", Once(let(modelPath)
                               .GLTF_Load_WithTransforms(btransform, atransform)
                                   Ref(model)
                               .Log())
                          .let(camera_pos) SetTable(cam, "Position")
                          .let(0.0, 0.0, 0.0) SetTable(cam, "Target")
                          .Get(cam)
                          .GFX_Camera()
                          .let(identity)
                          .GLTF_Draw(model));
    auto node = CBNode::make();
    node->schedule(test_chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick());
      if (node->empty())
        break;
      sleep(0.1);
    }
    auto errors = node->errors();
    REQUIRE(errors.size() == 0);
  }
}

void testLoad() {
  SECTION("Fail-Not-Existing") {
    DefChain(test_chain)
        .GFX_MainWindow("window", let("../Cube.gltf").GLTF_Load().Log());
    auto node = CBNode::make();
    node->schedule(test_chain);
    while (true) {
      if (!node->tick())
        break;
    }
    auto errors = node->errors();
    REQUIRE(errors[0] == "GLTF model file does not exist.");
  }

  SECTION("Cube2-Text") {
    DefChain(test_chain)
        .GFX_MainWindow(
            "window",
            let("../external/glTF-Sample-Models/2.0/Box/glTF/Box.gltf")
                .GLTF_Load_NoShaders()
                .Log());
    auto node = CBNode::make();
    node->schedule(test_chain);
    while (true) {
      REQUIRE(node->tick());
      if (node->empty())
        break;
      sleep(0.1);
    }
    auto errors = node->errors();
    REQUIRE(errors.size() == 0);
  }

  SECTION("Cube3-Text") {
    DefChain(test_chain)
        .GFX_MainWindow(
            "window",
            let("../external/glTF-Sample-Models/2.0/Box/glTF/Box.gltf")
                .GLTF_Load() // will load and compile shaders
                .Log());
    auto node = CBNode::make();
    node->schedule(test_chain);
    while (true) {
      REQUIRE(node->tick());
      if (node->empty())
        break;
      sleep(0.1);
    }
    auto errors = node->errors();
    REQUIRE(errors.size() == 0);
  }

  SECTION("Cube1-Text") {
    DefChain(test_chain)
        .Looped()
        .GFX_MainWindow("window",
                        Once(
                            // load the model
                            let("../deps/tinygltf/models/Cube/Cube.gltf")
                                .GLTF_Load_NoShaders() Ref(model)
                                .Log())
                            .let(0.0, 0.0, 10.0) SetTable(cam, "Position")
                            .let(0.0, 0.0, 0.0) SetTable(cam, "Target")
                            .Get(cam)
                            .GFX_Camera()
                            .let(identity)
                            .GLTF_Draw(model));
    auto node = CBNode::make();
    node->schedule(test_chain);
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

  testGLTFModel("Cube-Text", "../deps/tinygltf/models/Cube/Cube.gltf",
                "../deps/bgfx/examples/06-bump/fieldstone-rgba.tga",
                "../deps/bgfx/examples/06-bump/fieldstone-n.tga", "Cube",
                Var(10.0, 10.0, 10.0));

  testGLTFModel(
      "Avocado-Bin",
      "../external/glTF-Sample-Models/2.0/Avocado/glTF-Binary/Avocado.glb",
      "2256_Avocado_d", Var(0.1, 0.1, 0.1));

  testGLTFModel(
      "Duck-Text", "../external/glTF-Sample-Models/2.0/Duck/glTF/Duck.gltf",
      "../external/glTF-Sample-Models/2.0/Duck/glTF/DuckCM.png",
      "../external/glTF-Sample-Models/2.0/Avocado/glTF/Avocado_normal.png",
      "blinn3-fx", Var(2.0, 2.0, 2.0));

  testGLTFModel("Buggy-Text",
                "../external/glTF-Sample-Models/2.0/Buggy/glTF/Buggy.gltf",
                Var(100.0, 100.0, 100.0));

  testGLTFModel("BoxVertexColors-Text",
                "../external/glTF-Sample-Models/2.0/BoxVertexColors/glTF/"
                "BoxVertexColors.gltf",
                Var(0.0, 0.0, 2.0));

  testGLTFModel("TextureCoordinateTest-Text",
                "../external/glTF-Sample-Models/2.0/TextureCoordinateTest/glTF/"
                "TextureCoordinateTest.gltf",
                Var(0.0, 0.0, 4.0));

  testGLTFModel(
      "Avocado-Bin-2",
      "../external/glTF-Sample-Models/2.0/Avocado/glTF-Binary/Avocado.glb",
      Var(1.0, 1.0, 1.0), scale10);

  testGLTFModel("Sponza",
                "../external/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf",
                Var(0.0, 1.0, 1.0));
}
} // namespace GLTF_Tests
} // namespace chainblocks