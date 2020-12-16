#include <catch2/catch_all.hpp>

#include "../core/runtime.hpp"
#include "../../include/ops.hpp"

using namespace chainblocks;

TEST_CASE("Test type2Name", "[ops]" ) {
  REQUIRE_THROWS(type2Name(CBType::EndOfBlittableTypes));
  REQUIRE(type2Name(CBType::None) == "None");
  REQUIRE(type2Name(CBType::Any) == "Any");
  REQUIRE(type2Name(CBType::Object) == "Object");
  REQUIRE(type2Name(CBType::Enum) == "Enum");
  REQUIRE(type2Name(CBType::Bool) == "Bool");
  REQUIRE(type2Name(CBType::Bytes) == "Bytes");
  REQUIRE(type2Name(CBType::Color) == "Color");
  REQUIRE(type2Name(CBType::Int) == "Int");
  REQUIRE(type2Name(CBType::Int2) == "Int2");
  REQUIRE(type2Name(CBType::Int3) == "Int3");
  REQUIRE(type2Name(CBType::Int4) == "Int4");
  REQUIRE(type2Name(CBType::Int8) == "Int8");
  REQUIRE(type2Name(CBType::Int16) == "Int16");
  REQUIRE(type2Name(CBType::Float) == "Float");
  REQUIRE(type2Name(CBType::Float2) == "Float2");
  REQUIRE(type2Name(CBType::Float3) == "Float3");
  REQUIRE(type2Name(CBType::Float4) == "Float4");
  REQUIRE(type2Name(CBType::Block) == "Block");
  REQUIRE(type2Name(CBType::String) == "String");
  REQUIRE(type2Name(CBType::ContextVar) == "ContextVar");
  REQUIRE(type2Name(CBType::Path) == "Path");
  REQUIRE(type2Name(CBType::Image) == "Image");
  REQUIRE(type2Name(CBType::Seq) == "Seq");
  REQUIRE(type2Name(CBType::Table) == "Table");
  REQUIRE(type2Name(CBType::Array) == "Array");
}

TEST_CASE("Test operator==", "[ops]") {
  SECTION("None, Any, EndOfBlittableTypes") {
    auto n1 = Var::Empty;
    auto n2 = Var::Empty;
    auto n3 = Var(100);
    REQUIRE(n1 == n2);
    REQUIRE(n1 != n3);
  }

  SECTION("Object") {
    int x = 10;
    int y = 20;
    auto o1 = Var::Object(&x, 10, 20);
    auto o2 = Var::Object(&x, 10, 20);
    auto o3 = Var::Object(&y, 10, 20);
    auto o4 = Var::Object(&y, 11, 20);
    auto empty = Var::Empty;
    REQUIRE(o1 == o2);
    REQUIRE(o1 != o3);
    REQUIRE(o3 != o4);
    REQUIRE(o1 != empty);
  }
}