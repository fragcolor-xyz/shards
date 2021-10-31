/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

// included in bgfx.cpp

#ifndef CB_INTERNAL_TESTS
// just for linting purposes
#include "./bgfx.cpp"
#endif

#include <fstream>

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

namespace chainblocks {
namespace BGFX_Tests {
void testVertexAttribute() {
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Position) ==
          bgfx::Attrib::Position);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Normal) ==
          bgfx::Attrib::Normal);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Tangent3) ==
          bgfx::Attrib::Tangent);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Tangent4) ==
          bgfx::Attrib::Tangent);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Bitangent) ==
          bgfx::Attrib::Bitangent);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Color0) ==
          bgfx::Attrib::Color0);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Color1) ==
          bgfx::Attrib::Color1);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Color2) ==
          bgfx::Attrib::Color2);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Color3) ==
          bgfx::Attrib::Color3);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Indices) ==
          bgfx::Attrib::Indices);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Weight) ==
          bgfx::Attrib::Weight);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord0) ==
          bgfx::Attrib::TexCoord0);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord1) ==
          bgfx::Attrib::TexCoord1);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord2) ==
          bgfx::Attrib::TexCoord2);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord3) ==
          bgfx::Attrib::TexCoord3);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord4) ==
          bgfx::Attrib::TexCoord4);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord5) ==
          bgfx::Attrib::TexCoord5);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord6) ==
          bgfx::Attrib::TexCoord6);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord7) ==
          bgfx::Attrib::TexCoord7);
  REQUIRE_THROWS(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Skip) ==
                 bgfx::Attrib::TexCoord7);
}

void testModel() {
  std::vector<Var> layout = {
      Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
      Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

  SECTION("Working") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(cubeVertices)
                                .block("Set", "cube", "Vertices")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout), false,
                                       Var::Enum(BGFX::Enums::CullMode::Front,
                                                 CoreCC, 'gfxV')));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }

  SECTION("Fail-NoCtx") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .let(cubeVertices)
            .block("Set", "cube", "Vertices")
            .let(cubeIndices)
            .block("Set", "cube", "Indices")
            .block("Get", "cube")
            .block("GFX.Model", Var(layout), false,
                   Var::Enum(BGFX::Enums::CullMode::Front, CoreCC, 'gfxV'));
    auto node = CBNode::make();
    auto failed = false;
    try {
      node->schedule(chain);
    } catch (const ComposeError &ex) {
      CBLOG_ERROR("Compose error: {}", ex.what());
      failed = true;
    }
    REQUIRE(failed);
  }

  SECTION("Fail-Compose1") {
    std::vector<Var> cubeVertices = {
        Var(1.0, 1.0),         Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let("foobar")
                                .block("Set", "cube", "Vertices")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout), false,
                                       Var::Enum(BGFX::Enums::CullMode::Front,
                                                 CoreCC, 'gfxV')));
    auto node = CBNode::make();
    auto failed = false;
    try {
      node->schedule(chain);
    } catch (const ComposeError &ex) {
      CBLOG_ERROR("Compose error: {}", ex.what());
      failed = true;
    }
    REQUIRE(failed);
  }

  SECTION("Fail-Compose2") {
    std::vector<Var> cubeVertices = {
        Var(1.0, 1.0),         Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(cubeVertices)
                                .block("Set", "cube", "foo")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout), false,
                                       Var::Enum(BGFX::Enums::CullMode::Front,
                                                 CoreCC, 'gfxV')));
    auto node = CBNode::make();
    auto failed = false;
    try {
      node->schedule(chain);
    } catch (const ComposeError &ex) {
      CBLOG_ERROR("Compose error: {}", ex.what());
      failed = true;
    }
    REQUIRE(failed);
  }

  SECTION("Fail-Compose3") {
    std::vector<Var> cubeVertices = {
        Var(1.0, 1.0),         Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks().block(
                    "Await", Blocks()
                                 .let(cubeVertices)
                                 .block("Set", "cube", "Vertices")
                                 .let(cubeIndices)
                                 .block("Set", "cube", "Indices")
                                 .block("Get", "cube")
                                 .block("GFX.Model", Var(layout), false,
                                        Var::Enum(BGFX::Enums::CullMode::Front,
                                                  CoreCC, 'gfxV'))));
    auto node = CBNode::make();
    auto failed = false;
    try {
      node->schedule(chain);
    } catch (const ComposeError &ex) {
      CBLOG_ERROR("Compose error: {}", ex.what());
      failed = true;
    }
    REQUIRE(failed);
  }

  SECTION("Fail-Activate1") {
    std::vector<Var> cubeVertices = {
        Var(1.0, 1.0),         Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(cubeVertices)
                                .block("Set", "cube", "Vertices")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout), false,
                                       Var::Enum(BGFX::Enums::CullMode::Front,
                                                 CoreCC, 'gfxV')));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    CBLOG_ERROR(errors[0]);
    REQUIRE(errors[0] == "Invalid vertex buffer element type");
  }

  SECTION("Fail-Activate2") {
    std::vector<Var> cubeVertices = {
        Var(1.0, 1.0),        Var(1.0, 1.0, 1.0),   Var(0xff0000ff),
        Var(-1.0, -1.0, 1.0), Var(0xff00ff00),      Var(1.0, -1.0, 1.0),
        Var(0xff00ffff),      Var(-1.0, 1.0, -1.0), Var(0xffff0000),
        Var(1.0, 1.0, -1.0),  Var(0xffff00ff),      Var(-1.0, -1.0, -1.0),
        Var(0xffffff00),      Var(1.0, -1.0, -1.0), Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(cubeVertices)
                                .block("Set", "cube", "Vertices")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout), false,
                                       Var::Enum(BGFX::Enums::CullMode::Front,
                                                 CoreCC, 'gfxV')));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    CBLOG_ERROR(errors[0]);
    REQUIRE(errors[0] == "Invalid amount of vertex buffer elements");
  }

  SECTION("Fail-Activate3") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(-1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6),  Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1),  Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(cubeVertices)
                                .block("Set", "cube", "Vertices")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout), false,
                                       Var::Enum(BGFX::Enums::CullMode::Front,
                                                 CoreCC, 'gfxV')));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    CBLOG_ERROR(errors[0]);
    REQUIRE(errors[0] == "Vertex index out of range");
  }

  SECTION("Fail-Activate4") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2),   Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(300, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1),   Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(cubeVertices)
                                .block("Set", "cube", "Vertices")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout), false,
                                       Var::Enum(BGFX::Enums::CullMode::Front,
                                                 CoreCC, 'gfxV')));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    CBLOG_ERROR(errors[0]);
    REQUIRE(errors[0] == "Vertex index out of range");
  }
}

void testCamera() {
  SECTION("Working") {
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(0.0, 0.0, 0.0)
                                .block("Set", "cam", "Position")
                                .let(0.0, 0.0, 35.0)
                                .block("Set", "cam", "Target")
                                .block("Get", "cam")
                                .block("GFX.Camera"));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }
}

void testDraw() {
  SECTION("Working") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_cubes.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_cubes.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block("Once",
                           Blocks() //
                               .let(vs)
                               .block("FS.Read", true)
                               .block("Set", "vs_bytes")
                               .let(fs)
                               .block("FS.Read", true)
                               .block("Set", "fs_bytes")
                               .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                      Var::ContextVar("fs_bytes"))
                               .block("Set", "shader")
                               .let(cubeVertices)
                               .block("Set", "cube", "Vertices")
                               .let(cubeIndices)
                               .block("Set", "cube", "Indices")
                               .block("Get", "cube")
                               .block("GFX.Model", Var(layout), false,
                                      Var::Enum(BGFX::Enums::CullMode::Front,
                                                CoreCC, 'gfxV'))
                               .block("Set", "cube-model"))
                    .let(0.0, 0.0, 10.0)
                    .block("Set", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Set", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"), Var::Any,
                           Var::ContextVar("cube-model")));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }
}

void testUniforms() {
  SECTION("Working Float4 Array") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    std::vector<Var> uniformParamsArray = {
        Var(1.0, 0.7, 0.2, 0.8), Var(0.7, 0.2, 1.0, 0.8),
        Var(0.2, 1.0, 0.7, 0.8), Var(1.0, 0.4, 0.2, 0.8)};

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_bump.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_bump.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block(
                        "Once",
                        Blocks() //
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-rgba.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-n.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .let(vs)
                            .block("FS.Read", true)
                            .block("Ref", "vs_bytes")
                            .let(fs)
                            .block("FS.Read", true)
                            .block("Ref", "fs_bytes")
                            .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                   Var::ContextVar("fs_bytes"))
                            .block("Ref", "shader")
                            .let(cubeVertices)
                            .block("Ref", "cube", "Vertices")
                            .let(cubeIndices)
                            .block("Ref", "cube", "Indices")
                            .block("Get", "cube")
                            .block("GFX.Model", Var(layout), false,
                                   Var::Enum(BGFX::Enums::CullMode::Front,
                                             CoreCC, 'gfxV'))
                            .block("Ref", "cube-model"))
                    .let(uniformParamsArray)
                    .GFX_SetUniform("u_lightPosRadius", 4)
                    .let(uniformParamsArray)
                    .GFX_SetUniform("u_lightRgbInnerR", 4)
                    .let(10.0, 10.0, 10.0)
                    .block("Ref", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Ref", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"),
                           Var::ContextVar("textures"),
                           Var::ContextVar("cube-model")));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }

  SECTION("Working Mat4 Array") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    std::vector<Var> uniformParamsArray = {
        Var(1.0, 0.7, 0.2, 0.8), Var(0.7, 0.2, 1.0, 0.8),
        Var(0.2, 1.0, 0.7, 0.8), Var(1.0, 0.4, 0.2, 0.8)};

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_bump.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_bump.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block(
                        "Once",
                        Blocks() //
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-rgba.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-n.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .let(vs)
                            .block("FS.Read", true)
                            .block("Ref", "vs_bytes")
                            .let(fs)
                            .block("FS.Read", true)
                            .block("Ref", "fs_bytes")
                            .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                   Var::ContextVar("fs_bytes"))
                            .block("Ref", "shader")
                            .let(cubeVertices)
                            .block("Ref", "cube", "Vertices")
                            .let(cubeIndices)
                            .block("Ref", "cube", "Indices")
                            .block("Get", "cube")
                            .block("GFX.Model", Var(layout), false,
                                   Var::Enum(BGFX::Enums::CullMode::Front,
                                             CoreCC, 'gfxV'))
                            .block("Ref", "cube-model"))
                    .let(uniformParamsArray)
                    .block("Push", "matricesX2")
                    .block("Push", "matricesX2")
                    .block("Get", "matricesX2")
                    .GFX_SetUniform("u_lightPosRadius", 2)
                    .block("Get", "matricesX2")
                    .GFX_SetUniform("u_lightRgbInnerR", 2)
                    .let(10.0, 10.0, 10.0)
                    .block("Ref", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Ref", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"),
                           Var::ContextVar("textures"),
                           Var::ContextVar("cube-model")));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }

  SECTION("Working Mat3 Array") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    std::vector<Var> uniformParamsArray = {
        Var(1.0, 0.7, 0.2), Var(0.7, 0.2, 1.0), Var(0.2, 1.0, 0.7)};

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_bump.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_bump.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block(
                        "Once",
                        Blocks() //
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-rgba.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-n.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .let(vs)
                            .block("FS.Read", true)
                            .block("Ref", "vs_bytes")
                            .let(fs)
                            .block("FS.Read", true)
                            .block("Ref", "fs_bytes")
                            .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                   Var::ContextVar("fs_bytes"))
                            .block("Ref", "shader")
                            .let(cubeVertices)
                            .block("Ref", "cube", "Vertices")
                            .let(cubeIndices)
                            .block("Ref", "cube", "Indices")
                            .block("Get", "cube")
                            .block("GFX.Model", Var(layout), false,
                                   Var::Enum(BGFX::Enums::CullMode::Front,
                                             CoreCC, 'gfxV'))
                            .block("Ref", "cube-model"))
                    .let(uniformParamsArray)
                    .block("Push", "matricesX2")
                    .block("Push", "matricesX2")
                    .block("Get", "matricesX2")
                    .GFX_SetUniform("u_lightPosRadius", 2)
                    .block("Get", "matricesX2")
                    .GFX_SetUniform("u_lightRgbInnerR", 2)
                    .let(10.0, 10.0, 10.0)
                    .block("Ref", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Ref", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"),
                           Var::ContextVar("textures"),
                           Var::ContextVar("cube-model")));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }

  SECTION("Working Float4") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    Var uniformParams(1.0, 0.7, 0.2, 0.8);

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_bump.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_bump.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block(
                        "Once",
                        Blocks() //
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-rgba.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-n.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .let(vs)
                            .block("FS.Read", true)
                            .block("Ref", "vs_bytes")
                            .let(fs)
                            .block("FS.Read", true)
                            .block("Ref", "fs_bytes")
                            .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                   Var::ContextVar("fs_bytes"))
                            .block("Ref", "shader")
                            .let(cubeVertices)
                            .block("Ref", "cube", "Vertices")
                            .let(cubeIndices)
                            .block("Ref", "cube", "Indices")
                            .block("Get", "cube")
                            .block("GFX.Model", Var(layout), false,
                                   Var::Enum(BGFX::Enums::CullMode::Front,
                                             CoreCC, 'gfxV'))
                            .block("Ref", "cube-model"))
                    .let(uniformParams) // wrong actually
                    .GFX_SetUniform("u_lightPosRadius", 1)
                    .let(uniformParams) // wrong actually
                    .GFX_SetUniform("u_lightRgbInnerR", 1)
                    .let(10.0, 10.0, 10.0)
                    .block("Ref", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Ref", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"),
                           Var::ContextVar("textures"),
                           Var::ContextVar("cube-model")));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }

  SECTION("Working Mat4") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    std::vector<Var> uniformParams = {
        Var(1.0, 0.7, 0.2, 0.8), Var(0.7, 0.2, 1.0, 0.8),
        Var(0.2, 1.0, 0.7, 0.8), Var(1.0, 0.4, 0.2, 0.8)};

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_bump.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_bump.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block(
                        "Once",
                        Blocks() //
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-rgba.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-n.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .let(vs)
                            .block("FS.Read", true)
                            .block("Ref", "vs_bytes")
                            .let(fs)
                            .block("FS.Read", true)
                            .block("Ref", "fs_bytes")
                            .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                   Var::ContextVar("fs_bytes"))
                            .block("Ref", "shader")
                            .let(cubeVertices)
                            .block("Ref", "cube", "Vertices")
                            .let(cubeIndices)
                            .block("Ref", "cube", "Indices")
                            .block("Get", "cube")
                            .block("GFX.Model", Var(layout), false,
                                   Var::Enum(BGFX::Enums::CullMode::Front,
                                             CoreCC, 'gfxV'))
                            .block("Ref", "cube-model"))
                    .let(uniformParams) // wrong actually
                    .GFX_SetUniform("u_lightPosRadius", 1)
                    .let(uniformParams) // wrong actually
                    .GFX_SetUniform("u_lightRgbInnerR", 1)
                    .let(10.0, 10.0, 10.0)
                    .block("Ref", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Ref", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"),
                           Var::ContextVar("textures"),
                           Var::ContextVar("cube-model")));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }

  SECTION("Working Mat3") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    std::vector<Var> uniformParams = {Var(1.0, 0.7, 0.2), Var(0.7, 0.2, 1.0),
                                      Var(0.2, 1.0, 0.7)};

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_bump.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_bump.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block(
                        "Once",
                        Blocks() //
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-rgba.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .block("LoadImage", "../deps/bgfx/examples/06-bump/"
                                                "fieldstone-n.tga")
                            .GFX_Texture2D()
                            .block("Push", "textures")
                            .let(vs)
                            .block("FS.Read", true)
                            .block("Ref", "vs_bytes")
                            .let(fs)
                            .block("FS.Read", true)
                            .block("Ref", "fs_bytes")
                            .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                   Var::ContextVar("fs_bytes"))
                            .block("Ref", "shader")
                            .let(cubeVertices)
                            .block("Ref", "cube", "Vertices")
                            .let(cubeIndices)
                            .block("Ref", "cube", "Indices")
                            .block("Get", "cube")
                            .block("GFX.Model", Var(layout), false,
                                   Var::Enum(BGFX::Enums::CullMode::Front,
                                             CoreCC, 'gfxV'))
                            .block("Ref", "cube-model"))
                    .let(uniformParams) // wrong actually
                    .GFX_SetUniform("u_lightPosRadius", 1)
                    .let(uniformParams) // wrong actually
                    .GFX_SetUniform("u_lightRgbInnerR", 1)
                    .let(10.0, 10.0, 10.0)
                    .block("Ref", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Ref", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"),
                           Var::ContextVar("textures"),
                           Var::ContextVar("cube-model")));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }
}

void testShaderCompiler() {
  SECTION("Create") {
    auto compiler = makeShaderCompiler();
    REQUIRE(compiler);
  }

  SECTION("Compile") {
    std::ifstream va("../src/extra/shaders/var_cubes.txt");
    std::stringstream vabuffer;
    vabuffer << va.rdbuf();
    auto vastr = vabuffer.str();

    std::ifstream code("../src/extra/shaders/vs_cubes.glsl");
    std::stringstream codebuffer;
    codebuffer << code.rdbuf();
    auto codestr = codebuffer.str();

    auto compiler = makeShaderCompiler();
    auto bytes = compiler->compile(vastr, codestr, "v", "A=1;B=2;C=3", nullptr);
    REQUIRE(bytes.payload.bytesSize > 0);

    DefChain(test_chain)
        .Looped()
        .let(vastr) SetTable(in, "varyings")
        .let(codestr) SetTable(in, "code")
        .let("A") Push(ds)
        .let("B") Push(ds)
        .Get(ds) SetTable(in, "defines")
        .Get(in)
        .GFX_CompileShader();
    auto node = CBNode::make();
    node->schedule(test_chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }
}

void testScreenshot() {
  SECTION("Screenshot") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_cubes.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_cubes.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block("Once",
                           Blocks() //
                               .let(vs)
                               .block("FS.Read", true)
                               .block("Set", "vs_bytes")
                               .let(fs)
                               .block("FS.Read", true)
                               .block("Set", "fs_bytes")
                               .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                      Var::ContextVar("fs_bytes"))
                               .block("Set", "shader")
                               .let(cubeVertices)
                               .block("Set", "cube", "Vertices")
                               .let(cubeIndices)
                               .block("Set", "cube", "Indices")
                               .block("Get", "cube")
                               .block("GFX.Model", Var(layout), false,
                                      Var::Enum(BGFX::Enums::CullMode::Front,
                                                CoreCC, 'gfxV'))
                               .block("Set", "cube-model")
                               .let("screenshot.png")
                               .block("GFX.Screenshot"))
                    .let(0.0, 0.0, 10.0)
                    .block("Set", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Set", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"), Var::Any,
                           Var::ContextVar("cube-model")));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }

  SECTION("Screenshot-jpg") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };

    std::vector<Var> layout = {
        Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
        Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

    std::vector<Var> identity = {
        Var(1.0, 0.0, 0.0, 0.0), Var(0.0, 1.0, 0.0, 0.0),
        Var(0.0, 0.0, 1.0, 0.0), Var(0.0, 0.0, 0.0, 1.0)};

    const auto vs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/vs_cubes.bin";
    const auto fs =
        "../deps/bgfx/examples/runtime/shaders/" SHADERS_FOLDER "/fs_cubes.bin";

    auto chain =
        chainblocks::Chain("test-chain")
            .looped(true)
            .block(
                "GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                Blocks()
                    .block("Once",
                           Blocks() //
                               .let(vs)
                               .block("FS.Read", true)
                               .block("Set", "vs_bytes")
                               .let(fs)
                               .block("FS.Read", true)
                               .block("Set", "fs_bytes")
                               .block("GFX.Shader", Var::ContextVar("vs_bytes"),
                                      Var::ContextVar("fs_bytes"))
                               .block("Set", "shader")
                               .let(cubeVertices)
                               .block("Set", "cube", "Vertices")
                               .let(cubeIndices)
                               .block("Set", "cube", "Indices")
                               .block("Get", "cube")
                               .block("GFX.Model", Var(layout), false,
                                      Var::Enum(BGFX::Enums::CullMode::Front,
                                                CoreCC, 'gfxV'))
                               .block("Set", "cube-model"))
                    .let(0.0, 0.0, 10.0)
                    .block("Set", "cam", "Position")
                    .let(0.0, 0.0, 0.0)
                    .block("Set", "cam", "Target")
                    .block("Get", "cam")
                    .block("GFX.Camera")
                    .let(identity)
                    .block("GFX.Draw", Var::ContextVar("shader"), Var::Any,
                           Var::ContextVar("cube-model"))
                    .let("screenshot.jpg")
                    .block("GFX.Screenshot", false));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 5;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.1);
    }
  }
}

} // namespace BGFX_Tests
} // namespace chainblocks
