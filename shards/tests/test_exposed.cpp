#include <catch2/catch_all.hpp>
#include <shards/core/runtime.hpp>
#include <shards/lang/bindings.h>

#define SH_WIRE_DSL_DEFINES
#include <shards/wire_dsl.hpp>

using namespace shards;

struct ParsedWire {
  static std::shared_ptr<ParsedWire> make(std::string_view src) {
    auto result = std::shared_ptr<ParsedWire>(new ParsedWire());
    auto seq = shards_read(SHStringWithLen{}, ToSWL(src), SHStringWithLen{});
    REQUIRE(seq.ast);
    result->_wire = shards_eval(seq.ast, SHStringWithLen{"root", strlen("root")});
    shards_free_sequence(seq.ast);
    REQUIRE(result->_wire.wire);
    return result;
  }
  std::shared_ptr<SHWire> wire() { return SHWire::sharedFromRef(*(_wire.wire)); }
  ~ParsedWire() { shards_free_wire(_wire.wire); }

private:
  ParsedWire() = default;
  SHLWire _wire;
};

TEST_CASE("Set exposed", "[variable]") {
  shards_init(shardsInterface(SHARDS_CURRENT_ABI));

  Var c0 = Var(0);
  Var c1 = Var(1);

  auto wire = ParsedWire::make(R"(
      0 | Set(v0 Exposed: true)
      v0 | Math.Add(1) > v0
      Log("a")
  )");
  wire->wire()->looped = true;
  auto mesh = SHMesh::make();

  struct Handler {
    size_t warmupCounter{};
    size_t setCounter{};
    void warmup(OnExposedVarWarmup &e) {
      if (e.name == "v0") {
        warmupCounter++;
      }
    }
    void set(OnExposedVarSet &e) {
      if (e.name == "v0") {
        setCounter++;
      }
    }
  } h;
  wire->wire()->dispatcher.sink<OnExposedVarWarmup>().connect<&Handler::warmup>(&h);
  wire->wire()->dispatcher.sink<OnExposedVarSet>().connect<&Handler::set>(&h);
  DEFER({ mesh->terminate(); });
  mesh->schedule(wire->wire());
  for (size_t i = 0; i < 1; i++) {
    REQUIRE(mesh->tick());
  }

  REQUIRE(h.warmupCounter == 1);
  REQUIRE(h.setCounter == 2);
}