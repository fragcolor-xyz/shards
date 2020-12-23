#include <random>

#include "../../include/ops.hpp"
#include "../../include/utility.hpp"
#include "../core/runtime.hpp"

#undef CHECK

#include <catch2/catch_all.hpp>

namespace chainblocks {
namespace BGFX_Tests {
  extern void testVertexAttribute();
}
}

using namespace chainblocks;

TEST_CASE("BGFX-VertexAttribute", "[gfx]") {
  BGFX_Tests::testVertexAttribute();
}