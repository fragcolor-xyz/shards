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

#include <chain_dsl.hpp>

#define GLTF_TEST_MODEL(_name, _path, _albedo, _normal, _mat_name, _cam_scale) \
  SECTION(_name) {                                                             \
    DefChain(test_chain)                                                       \
        .Looped()                                                              \
        .GFX_MainWindow("window", Once(let(_path)                              \
                                           .GLTF_Load_NoBitangents()           \
                                           .Ref(model)                         \
                                           .Log()                              \
                                           .LoadImage(_albedo)                 \
                                           .GFX_Texture2D()                    \
                                           .Push(textures)                     \
                                           .LoadImage(_normal)                 \
                                           .GFX_Texture2D()                    \
                                           .Push(textures)                     \
                                           .let(vs)                            \
                                           .FS_Read_Bytes()                    \
                                           .Ref(vs_bytes)                      \
                                           .let(fs)                            \
                                           .FS_Read_Bytes()                    \
                                           .Ref(fs_bytes)                      \
                                           .GFX_Shader(vs_bytes, fs_bytes)     \
                                           .RefTable(mat1, "Shader")           \
                                           .Get(textures)                      \
                                           .RefTable(mat1, "Textures")         \
                                           .Get(mat1)                          \
                                           .RefTable(mats, _mat_name))         \
                                      .let(_cam_scale, _cam_scale, _cam_scale) \
                                      .RefTable(cam, "Position")               \
                                      .let(0.0, 0.0, 0.0)                      \
                                      .RefTable(cam, "Target")                 \
                                      .Get(cam)                                \
                                      .GFX_Camera()                            \
                                      .let(identity)                           \
                                      .GLTF_Draw_WithMaterials(model, mats));  \
    auto node = CBNode::make();                                                \
    node->schedule(test_chain);                                                \
    auto count = 50;                                                           \
    while (count--) {                                                          \
      REQUIRE(node->tick());                                                   \
      if (node->empty())                                                       \
        break;                                                                 \
      sleep(0.1);                                                              \
    }                                                                          \
    auto errors = node->errors();                                              \
    REQUIRE(errors.size() == 0);                                               \
  }

#define GLTF_TEST_MODEL2(_name, _path, _mat_name, _cam_scale)                  \
  SECTION(_name) {                                                             \
    DefChain(test_chain)                                                       \
        .Looped()                                                              \
        .GFX_MainWindow("window", Once(let(_path)                              \
                                           .GLTF_Load_NoBitangents()           \
                                           .Ref(model)                         \
                                           .Log()                              \
                                           .let(vs)                            \
                                           .FS_Read_Bytes()                    \
                                           .Ref(vs_bytes)                      \
                                           .let(fs)                            \
                                           .FS_Read_Bytes()                    \
                                           .Ref(fs_bytes)                      \
                                           .GFX_Shader(vs_bytes, fs_bytes)     \
                                           .RefTable(mat1, "Shader")           \
                                           .Get(mat1)                          \
                                           .RefTable(mats, _mat_name))         \
                                      .let(_cam_scale, _cam_scale, _cam_scale) \
                                      .RefTable(cam, "Position")               \
                                      .let(0.0, 0.0, 0.0)                      \
                                      .RefTable(cam, "Target")                 \
                                      .Get(cam)                                \
                                      .GFX_Camera()                            \
                                      .let(identity)                           \
                                      .GLTF_Draw_WithMaterials(model, mats));  \
    auto node = CBNode::make();                                                \
    node->schedule(test_chain);                                                \
    auto count = 50;                                                           \
    while (count--) {                                                          \
      REQUIRE(node->tick());                                                   \
      if (node->empty())                                                       \
        break;                                                                 \
      sleep(0.1);                                                              \
    }                                                                          \
    auto errors = node->errors();                                              \
    REQUIRE(errors.size() == 0);                                               \
  }

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

  SECTION("Cube1-Text") {
    DefChain(test_chain)
        .Looped()
        .GFX_MainWindow("window",
                        Once(
                            // load the model
                            let("../deps/tinygltf/models/Cube/Cube.gltf")
                                .GLTF_Load()
                                .Ref(model)
                                .Log())
                            .let(0.0, 0.0, 10.0)
                            .RefTable(cam, "Position")
                            .let(0.0, 0.0, 0.0)
                            .RefTable(cam, "Target")
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

  GLTF_TEST_MODEL(
      "Cube1-Text-NoBitangents", "../deps/tinygltf/models/Cube/Cube.gltf",
      "../deps/bgfx/examples/06-bump/fieldstone-rgba.tga",
      "../deps/bgfx/examples/06-bump/fieldstone-n.tga", "Cube", 10.0);

  GLTF_TEST_MODEL2(
      "Avocado-Bin-NoBitangents",
      "../external/glTF-Sample-Models/2.0/Avocado/glTF-Binary/Avocado.glb",
      "2256_Avocado_d", 0.1);

  GLTF_TEST_MODEL(
      "Duck-Text-NoBitangents",
      "../external/glTF-Sample-Models/2.0/Duck/glTF/Duck.gltf",
      "../external/glTF-Sample-Models/2.0/Duck/glTF/DuckCM.png",
      "../external/glTF-Sample-Models/2.0/Avocado/glTF/Avocado_normal.png",
      "blinn3-fx", 2.0);

  GLTF_TEST_MODEL(
      "Buggy-Text-NoBitangents",
      "../external/glTF-Sample-Models/2.0/Buggy/glTF/Buggy.gltf",
      "../external/glTF-Sample-Models/2.0/Duck/glTF/DuckCM.png",
      "../external/glTF-Sample-Models/2.0/Avocado/glTF/Avocado_normal.png",
      "Material_20", 100.0);

  SECTION("Cube2-Text") {
    DefChain(test_chain)
        .GFX_MainWindow(
            "window",
            let("../external/glTF-Sample-Models/2.0/Box/glTF/Box.gltf")
                .GLTF_Load()
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
}
} // namespace GLTF_Tests
} // namespace chainblocks