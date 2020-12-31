#include <random>

#include "../../include/ops.hpp"
#include "../../include/utility.hpp"
#include "../core/runtime.hpp"

#undef CHECK

#include <catch2/catch_all.hpp>

namespace chainblocks {
namespace BGFX_Tests {
  extern void testVertexAttribute();
  extern void testModel();
  extern void testCamera();
  extern void testDraw();
}
}

using namespace chainblocks;

TEST_CASE("GFX-VertexAttribute", "[gfx]") {
  BGFX_Tests::testVertexAttribute();
}

TEST_CASE("GFX-Model", "[gfx]") {
  BGFX_Tests::testModel();
}

TEST_CASE("GFX-Camera", "[gfx]") {
  BGFX_Tests::testCamera();
}

TEST_CASE("GFX-Draw", "[gfx]") {
  BGFX_Tests::testDraw();
}
