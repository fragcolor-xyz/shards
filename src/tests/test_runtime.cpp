#include <catch2/catch_all.hpp>

#include "../../include/ops.hpp"
#include "../core/runtime.hpp"

using namespace chainblocks;

TEST_CASE("Test type2Name", "[ops]") {
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

TEST_CASE("Test-Comparison", "[ops]") {
  SECTION("None, Any, EndOfBlittableTypes") {
    auto n1 = Var::Empty;
    auto n2 = Var::Empty;
    auto n3 = Var(100);
    REQUIRE(n1 == n2);
    REQUIRE(n1 != n3);
    auto hash1 = hash(n1);
    auto hash2 = hash(n2);
    REQUIRE(hash1 == hash2);
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
    auto hash1 = hash(o1);
    auto hash2 = hash(o2);
    auto hash3 = hash(o3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << o1; // logging coverage
  }

  SECTION("Enum") {
    int x = 10;
    int y = 20;
    auto o1 = Var::Enum(x, 10, 20);
    auto o2 = Var::Enum(x, 10, 20);
    auto o3 = Var::Enum(y, 10, 20);
    auto o4 = Var::Enum(y, 11, 20);
    auto empty = Var::Empty;
    REQUIRE(o1 == o2);
    REQUIRE(o1 != o3);
    REQUIRE(o3 != o4);
    REQUIRE(o1 != empty);
    auto hash1 = hash(o1);
    auto hash2 = hash(o2);
    auto hash3 = hash(o3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << o1; // logging coverage
  }

  SECTION("Float") {
    auto f1 = Var(10.0);
    REQUIRE(f1.valueType == CBType::Float);
    float ff = float(f1);
    REQUIRE(ff == 10);
    REQUIRE_THROWS(int(f1));
    auto f2 = Var(10.0);
    auto f3 = Var(22.0);
    auto i1 = Var(10);
    REQUIRE(f1 == f2);
    REQUIRE(f1 != f3);
    REQUIRE(f1 != i1);
    REQUIRE(f1 <= f2);
    REQUIRE(f1 <= f3);
    REQUIRE(f1 >= f2);
    REQUIRE_FALSE(f1 >= f3);
    REQUIRE(f1 < f3);
    REQUIRE((f3 > f1 and f3 > f2));
    REQUIRE((f3 >= f1 and f3 >= f2));
    auto hash1 = hash(f1);
    auto hash2 = hash(f2);
    auto hash3 = hash(f3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << f1; // logging coverage
  }

  SECTION("Float2") {
    auto f1 = Var(10.0, 2.0);
    REQUIRE(f1.valueType == CBType::Float2);
    auto f2 = Var(10.0, 2.0);
    auto f3 = Var(22.0, 2.0);
    auto i1 = Var(10);
    REQUIRE(f1 == f2);
    REQUIRE(f1 != f3);
    REQUIRE(f1 != i1);
    REQUIRE(f1 <= f2);
    REQUIRE(f1 <= f3);
    REQUIRE(f1 >= f2);
    REQUIRE_FALSE(f1 >= f3);
    REQUIRE(f1 < f3);
    REQUIRE((f3 > f1 and f3 > f2));
    REQUIRE((f3 >= f1 and f3 >= f2));
    auto hash1 = hash(f1);
    auto hash2 = hash(f2);
    auto hash3 = hash(f3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << f1; // logging coverage
  }

  SECTION("Float3") {
    auto f1 = Var(10.0, 2.0, 3.0);
    REQUIRE(f1.valueType == CBType::Float3);
    auto f2 = Var(10.0, 2.0, 3.0);
    auto f3 = Var(22.0, 2.0, 1.0);
    auto f4 = Var(22.0, 2.0, 4.0);
    auto i1 = Var(10);
    REQUIRE(f1 == f2);
    REQUIRE(f1 != f3);
    REQUIRE(f1 != i1);
    REQUIRE(f1 <= f2);
    REQUIRE_FALSE(f1 <= f3);
    REQUIRE(f1 < f4);
    REQUIRE(f1 >= f2);
    REQUIRE(f1 < f3);
    REQUIRE((f3 > f1 and f3 > f2));
    REQUIRE((f4 >= f1 and f4 >= f2));
    auto hash1 = hash(f1);
    auto hash2 = hash(f2);
    auto hash3 = hash(f3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << f1; // logging coverage
  }

  SECTION("Int") {
    auto f1 = Var(10);
    REQUIRE(f1.valueType == CBType::Int);
    int ff = int(f1);
    REQUIRE(ff == 10);
    REQUIRE_NOTHROW(float(f1)); // will convert to float automatically
    auto f2 = Var(10);
    auto f3 = Var(22);
    auto i1 = Var(10.0);
    REQUIRE(f1 == f2);
    REQUIRE(f1 != f3);
    REQUIRE(f1 != i1);
    REQUIRE(f1 <= f2);
    REQUIRE(f1 <= f3);
    REQUIRE(f1 >= f2);
    REQUIRE_FALSE(f1 >= f3);
    REQUIRE(f1 < f3);
    REQUIRE((f3 > f1 and f3 > f2));
    REQUIRE((f3 >= f1 and f3 >= f2));
    auto hash1 = hash(f1);
    auto hash2 = hash(f2);
    auto hash3 = hash(f3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << f1; // logging coverage
  }

  SECTION("Int2") {
    auto f1 = Var(10, 2);
    REQUIRE(f1.valueType == CBType::Int2);
    auto f2 = Var(10, 2);
    auto f3 = Var(22, 2);
    auto i1 = Var(10.0);
    REQUIRE(f1 == f2);
    REQUIRE(f1 != f3);
    REQUIRE(f1 != i1);
    REQUIRE(f1 <= f2);
    REQUIRE(f1 <= f3);
    REQUIRE(f1 >= f2);
    REQUIRE_FALSE(f1 >= f3);
    REQUIRE(f1 < f3);
    REQUIRE((f3 > f1 and f3 > f2));
    REQUIRE((f3 >= f1 and f3 >= f2));
    auto hash1 = hash(f1);
    auto hash2 = hash(f2);
    auto hash3 = hash(f3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << f1; // logging coverage
  }

  SECTION("Int3") {
    auto f1 = Var(10, 2, 3);
    REQUIRE(f1.valueType == CBType::Int3);
    auto f2 = Var(10, 2, 3);
    auto f3 = Var(22, 2, 1);
    auto f4 = Var(22, 2, 4);
    auto i1 = Var(10);
    REQUIRE(f1 == f2);
    REQUIRE(f1 != f3);
    REQUIRE(f1 != i1);
    REQUIRE(f1 <= f2);
    REQUIRE_FALSE(f1 <= f3);
    REQUIRE(f1 < f4);
    REQUIRE(f1 >= f2);
    REQUIRE(f1 < f3);
    REQUIRE((f3 > f1 and f3 > f2));
    REQUIRE((f4 >= f1 and f4 >= f2));
    auto hash1 = hash(f1);
    auto hash2 = hash(f2);
    auto hash3 = hash(f3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << f1; // logging coverage
  }

  SECTION("String") {
    auto s1 = Var("Hello world");
    REQUIRE(s1.valueType == CBType::String);
    auto s2 = Var("Hello world");
    auto s3 = Var("Hello world 2");
    std::string q("qwerty");
    auto s4 = Var(q);
    REQUIRE_THROWS(float(s4));
    REQUIRE(s1 == s2);
    REQUIRE(s1 != s3);
    REQUIRE(s1 != s4);
    REQUIRE(s1 < s3);
    REQUIRE(s1 <= s3);
    REQUIRE(s3 >= s2);
    REQUIRE(s3 > s2);
    auto hash1 = hash(s1);
    auto hash2 = hash(s2);
    auto hash3 = hash(s3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << s1; // logging coverage
  }

  SECTION("Seq-Var") {
    std::vector<Var> s1{Var(10), Var(20), Var(30), Var(40), Var(50)};
    Var v1(s1);
    REQUIRE(v1.valueType == CBType::Seq);
    std::vector<Var> s2{Var(10), Var(20), Var(30), Var(40), Var(50)};
    Var v2(s2);
    REQUIRE(v2.valueType == CBType::Seq);
    std::vector<Var> s3{Var(100), Var(200), Var(300)};
    Var v3(s3);
    REQUIRE(v3.valueType == CBType::Seq);
    std::vector<Var> s4{Var(10), Var(20), Var(30)};
    Var v4(s4);
    REQUIRE(v4.valueType == CBType::Seq);
    std::vector<Var> s5{Var(1), Var(2), Var(3), Var(4), Var(5), Var(6)};
    Var v5(s5);
    REQUIRE(v5.valueType == CBType::Seq);
    std::vector<Var> s6{Var(10), Var(20), Var(30), Var(45), Var(55)};
    Var v6(s6);
    REQUIRE(v6.valueType == CBType::Seq);
    std::vector<Var> s7{Var(10), Var(20), Var(30), Var(25), Var(35)};
    Var v7(s7);
    REQUIRE(v7.valueType == CBType::Seq);
    std::vector<Var> s8{Var(10)};
    Var v8(s8);
    REQUIRE(v8.valueType == CBType::Seq);
    std::vector<Var> s9{Var(1)};
    Var v9(s9);
    REQUIRE(v9.valueType == CBType::Seq);

    REQUIRE_FALSE(v8 < v9);
    REQUIRE_FALSE(v8 <= v9);
    REQUIRE_FALSE(s8 < s9);
    REQUIRE_FALSE(s8 <= s9);
    REQUIRE(s1 == s2);
    REQUIRE(v1 == v2);
    REQUIRE_FALSE(v1 < v2);
    REQUIRE(v1 <= v2);
    REQUIRE_FALSE(s4 <= s8);
    REQUIRE_FALSE(v4 <= v8);
    REQUIRE(s1 <= s2);
    REQUIRE(s1 <= s2);
    REQUIRE(s1 != s5);
    REQUIRE(v1 != v5);
    REQUIRE(s1 != s6);
    REQUIRE(v1 != v6);
    REQUIRE(s1 != s3);
    REQUIRE(v1 != v3);
    REQUIRE(s2 != s3);
    REQUIRE(v2 != v3);
    REQUIRE(s3 > s1);
    REQUIRE(v3 > v1);
    REQUIRE(s1 < s3);
    REQUIRE(v1 < v3);
    REQUIRE(s2 > s4);
    REQUIRE(v2 > v4);
    REQUIRE(s2 > s5);
    REQUIRE(v2 > v5);
    REQUIRE(s3 >= s1);
    REQUIRE(v3 >= v1);
    REQUIRE(s1 <= s3);
    REQUIRE(v1 <= v3);
    REQUIRE(s2 >= s4);
    REQUIRE(v2 >= v4);
    REQUIRE(s2 >= s5);
    REQUIRE(v2 >= v5);
    REQUIRE(v6 > v1);
    REQUIRE(s6 > s1);
    REQUIRE(v6 >= v1);
    REQUIRE(s6 >= s1);
    REQUIRE(v1 < v6);
    REQUIRE(s1 < s6);
    REQUIRE(v1 <= v6);
    REQUIRE(s1 <= s6);
    REQUIRE_FALSE(v6 < v1);
    REQUIRE_FALSE(s6 < s1);
    REQUIRE_FALSE(v6 <= v1);
    REQUIRE_FALSE(s6 <= s1);
    REQUIRE_FALSE(v1 > v6);
    REQUIRE_FALSE(s1 > s6);
    REQUIRE_FALSE(v1 >= v6);
    REQUIRE_FALSE(s1 >= s6);
    REQUIRE_FALSE(v7 > v1);
    REQUIRE_FALSE(s7 > s1);
    REQUIRE_FALSE(v7 >= v1);
    REQUIRE_FALSE(s7 >= s1);
    REQUIRE_FALSE(v1 < v7);
    REQUIRE_FALSE(s1 < s7);
    REQUIRE_FALSE(v1 <= v7);
    REQUIRE_FALSE(s1 <= s7);
    REQUIRE(v7 < v1);
    REQUIRE(s7 < s1);
    REQUIRE(v7 <= v1);
    REQUIRE(s7 <= s1);
    REQUIRE(v1 > v7);
    REQUIRE(s1 > s7);
    REQUIRE(v1 >= v7);
    REQUIRE(s1 >= s7);

    LOG(INFO) << v1;
  }

  SECTION("Seq-CBVar") {
    std::vector<CBVar> s1{Var(10), Var(20), Var(30), Var(40), Var(50)};
    Var v1(s1);
    REQUIRE(v1.valueType == CBType::Seq);
    std::vector<CBVar> s2{Var(10), Var(20), Var(30), Var(40), Var(50)};
    Var v2(s2);
    REQUIRE(v2.valueType == CBType::Seq);
    std::vector<CBVar> s3{Var(100), Var(200), Var(300)};
    Var v3(s3);
    REQUIRE(v3.valueType == CBType::Seq);
    std::vector<CBVar> s4{Var(10), Var(20), Var(30)};
    Var v4(s4);
    REQUIRE(v4.valueType == CBType::Seq);
    std::vector<CBVar> s5{Var(1), Var(2), Var(3), Var(4), Var(5), Var(6)};
    Var v5(s5);
    REQUIRE(v5.valueType == CBType::Seq);

    REQUIRE(s1 == s2);
    REQUIRE(v1 == v2);
    REQUIRE(s1 != s3);
    REQUIRE(v1 != v3);
    REQUIRE(s2 != s3);
    REQUIRE(v2 != v3);
    REQUIRE(s3 > s1);
    REQUIRE(v3 > v1);
    REQUIRE(s1 < s3);
    REQUIRE(v1 < v3);
    REQUIRE(s2 > s4);
    REQUIRE(v2 > v4);
    REQUIRE(s2 > s5);
    REQUIRE(v2 > v5);
    REQUIRE(s3 >= s1);
    REQUIRE(v3 >= v1);
    REQUIRE(s1 <= s3);
    REQUIRE(v1 <= v3);
    REQUIRE(s2 >= s4);
    REQUIRE(v2 >= v4);
    REQUIRE(s2 >= s5);
    REQUIRE(v2 >= v5);

    LOG(INFO) << v1;
  }

  SECTION("Array-Var") {
    std::array<Var, 5> s1{Var(10), Var(20), Var(30), Var(40), Var(50)};
    Var v1(s1);
    REQUIRE(v1.valueType == CBType::Seq);
    std::array<Var, 5> s2{Var(10), Var(20), Var(30), Var(40), Var(50)};
    Var v2(s2);
    REQUIRE(v2.valueType == CBType::Seq);
    std::array<Var, 3> s3{Var(100), Var(200), Var(300)};
    Var v3(s3);
    REQUIRE(v3.valueType == CBType::Seq);
    std::array<Var, 3> s4{Var(10), Var(20), Var(30)};
    Var v4(s4);
    REQUIRE(v4.valueType == CBType::Seq);
    std::array<Var, 6> s5{Var(1), Var(2), Var(3), Var(4), Var(5), Var(6)};
    Var v5(s5);
    REQUIRE(v5.valueType == CBType::Seq);

    REQUIRE(v1 == v2);
    REQUIRE(v1 != v3);
    REQUIRE(v2 != v3);
    REQUIRE(v3 > v1);
    REQUIRE(v1 < v3);
    REQUIRE(v2 > v4);
    REQUIRE(v2 > v5);
    REQUIRE(v3 >= v1);
    REQUIRE(v1 <= v3);
    REQUIRE(v2 >= v4);
    REQUIRE(v2 >= v5);

    LOG(INFO) << v1;
  }

  SECTION("Array-CBVar") {
    std::array<CBVar, 5> s1{Var(10), Var(20), Var(30), Var(40), Var(50)};
    Var v1(s1);
    REQUIRE(v1.valueType == CBType::Seq);
    std::array<CBVar, 5> s2{Var(10), Var(20), Var(30), Var(40), Var(50)};
    Var v2(s2);
    REQUIRE(v2.valueType == CBType::Seq);
    std::array<CBVar, 3> s3{Var(100), Var(200), Var(300)};
    Var v3(s3);
    REQUIRE(v3.valueType == CBType::Seq);
    std::array<CBVar, 3> s4{Var(10), Var(20), Var(30)};
    Var v4(s4);
    REQUIRE(v4.valueType == CBType::Seq);
    std::array<CBVar, 6> s5{Var(1), Var(2), Var(3), Var(4), Var(5), Var(6)};
    Var v5(s5);
    REQUIRE(v5.valueType == CBType::Seq);

    REQUIRE(v1 == v2);
    REQUIRE(v1 != v3);
    REQUIRE(v2 != v3);
    REQUIRE(v3 > v1);
    REQUIRE(v1 < v3);
    REQUIRE(v2 > v4);
    REQUIRE(v2 > v5);
    REQUIRE(v3 >= v1);
    REQUIRE(v1 <= v3);
    REQUIRE(v2 >= v4);
    REQUIRE(v2 >= v5);

    LOG(INFO) << v1;
  }
}