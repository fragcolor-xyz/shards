/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

// included in bgfx.cpp

#ifdef CHECK
#undef CHECK
#endif

#include <catch2/catch_all.hpp>

namespace chainblocks {
namespace BGFX_Tests {
void testVertexAttribute() {
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Position) ==
          bgfx::Attrib::Position);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Normal) ==
          bgfx::Attrib::Normal);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Tangent) ==
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
  chainblocks::Globals::RootPath = "./";
  registerCoreBlocks();

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
                                .block("GFX.Model", Var(layout)));
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
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .let(cubeVertices)
                     .block("Set", "cube", "Vertices")
                     .let(cubeIndices)
                     .block("Set", "cube", "Indices")
                     .block("Get", "cube")
                     .block("GFX.Model", Var(layout));
    auto node = CBNode::make();
    auto failed = false;
    try {
      node->schedule(chain);
    } catch (const ComposeError &ex) {
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
                                .block("GFX.Model", Var(layout)));
    auto node = CBNode::make();
    auto failed = false;
    try {
      node->schedule(chain);
    } catch (const ComposeError &ex) {
      failed = true;
    }
    REQUIRE(failed);
  }

  // SECTION("Fail-Compose2") {
  //   std::vector<Var> cubeVertices = {
  //       Var(1.0, 1.0),         Var(0xff000000),      Var(1.0, 1.0, 1.0),
  //       Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
  //       Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
  //       Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
  //       Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
  //       Var(0xffffffff),
  //   };
  //   std::vector<Var> cubeIndices = {
  //       Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
  //       Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
  //       Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
  //   };
  //   auto chain = chainblocks::Chain("test-chain")
  //                    .looped(true)
  //                    .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
  //                           Blocks()
  //                               .let(cubeVertices)
  //                               .block("Set", "cube", "foo")
  //                               .let(cubeIndices)
  //                               .block("Set", "cube", "Indices")
  //                               .block("Get", "cube")
  //                               .block("GFX.Model", Var(layout)));
  //   auto node = CBNode::make();
  //   auto failed = false;
  //   try {
  //     node->schedule(chain);
  //   } catch (const ComposeError &ex) {
  //     failed = true;
  //   }
  //   REQUIRE(failed);
  // }

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
                                .block("GFX.Model", Var(layout)));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    LOG(ERROR) << errors[0];
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
                                .block("GFX.Model", Var(layout)));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    LOG(ERROR) << errors[0];
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
                                .block("GFX.Model", Var(layout)));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    LOG(ERROR) << errors[0];
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
                                .block("GFX.Model", Var(layout)));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    LOG(ERROR) << errors[0];
    REQUIRE(errors[0] == "Vertex index out of range");
  }
} // namespace BGFX_Tests

} // namespace BGFX_Tests
} // namespace chainblocks
