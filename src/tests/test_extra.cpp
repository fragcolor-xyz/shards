#include <random>

#include "../../include/ops.hpp"
#include "../../include/utility.hpp"
#include "../core/runtime.hpp"

#undef CHECK

#include <catch2/catch_all.hpp>

namespace chainblocks {
namespace BGFX_Tests {
  extern void testVertexAttribute();
  //extern void testModelInputLayoutPacking();
}
}

using namespace chainblocks;

TEST_CASE("BGFX-VertexAttribute", "[gfx]") {
  BGFX_Tests::testVertexAttribute();
}

// TEST_CASE("BGFX-ModelInputLayoutPacking", "[gfx]") {
//   BGFX_Tests::testModelInputLayoutPacking();
// }
