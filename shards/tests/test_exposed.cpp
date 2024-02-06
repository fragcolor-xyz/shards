#include <catch2/catch_all.hpp>
#include <shards/core/runtime.hpp>

#define SH_WIRE_DSL_DEFINES
#include <shards/wire_dsl.hpp>

using namespace shards;

TEST_CASE("Set exposed", "[variable]") {
  Var c0 = Var(0);
  Var c1 = Var(1);
  auto wire = shards::Wire("test").looped(true).let(c0).Set(v1).Get(v1);
  auto mesh = SHMesh::make();
  mesh->schedule(wire);
  for (size_t i = 0; i < 10; i++) {
    REQUIRE(mesh->tick());
  }
}