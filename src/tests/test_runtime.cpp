#include <random>

#include <catch2/catch_all.hpp>

#include "../../include/ops.hpp"
#include "../core/runtime.hpp"

using namespace chainblocks;

TEST_CASE("CBType-type2Name", "[ops]") {
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

TEST_CASE("CBVar-comparison", "[ops]") {
  SECTION("None, Any, EndOfBlittableTypes") {
    auto n1 = Var::Empty;
    auto n2 = Var::Empty;
    auto n3 = Var(100);
    REQUIRE(n1 == n2);
    REQUIRE(n1 != n3);
    auto hash1 = hash(n1);
    auto hash2 = hash(n2);
    REQUIRE(hash1 == hash2);
    LOG(INFO) << n1; // logging coverage
  }

  SECTION("Bool") {
    auto t = Var::True;
    auto f = Var::False;
    auto t1 = Var(true);
    auto f1 = Var(false);
    REQUIRE(t == t1);
    REQUIRE(f == f1);
    REQUIRE(true == bool(t1));
    REQUIRE_FALSE(true == bool(f1));
    REQUIRE(false == bool(f1));
    REQUIRE(t > f);
    REQUIRE(t >= f);
    REQUIRE(f < t);
    REQUIRE(f <= t);
    auto hash1 = hash(t);
    auto hash2 = hash(t1);
    REQUIRE(hash1 == hash2);
    LOG(INFO) << t; // logging coverage
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
    REQUIRE_THROWS(o3 > o4);
    REQUIRE_THROWS(o3 >= o4);
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
    REQUIRE(o1 < o3);
    REQUIRE_THROWS(o3 < o4);
    REQUIRE(o1 <= o3);
    REQUIRE_THROWS(o3 <= o4);
    REQUIRE_FALSE(o1 >= o3);
    REQUIRE(o3 >= o1);
    REQUIRE_THROWS(o3 >= o4);
    REQUIRE_THROWS(o4 >= o3);
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

#define CBVECTOR_TESTS                                                         \
  REQUIRE(f1 == f2);                                                           \
  REQUIRE(v1 == v2);                                                           \
  REQUIRE(f1 != f3);                                                           \
  REQUIRE(v1 != v3);                                                           \
  REQUIRE(f1 != i1);                                                           \
  REQUIRE_THROWS(f1 < i1);                                                     \
  REQUIRE_THROWS(f1 <= i1);                                                    \
  REQUIRE(f1 <= f2);                                                           \
  REQUIRE(v1 <= v2);                                                           \
  REQUIRE_FALSE(f3 <= f1);                                                     \
  REQUIRE_FALSE(v3 <= v1);                                                     \
  REQUIRE(f1 < f4);                                                            \
  REQUIRE(v1 < v4);                                                            \
  REQUIRE(f1 >= f2);                                                           \
  REQUIRE(v1 >= v2);                                                           \
  REQUIRE(f1 < f3);                                                            \
  REQUIRE(v1 < v3);                                                            \
  REQUIRE((f3 > f1 and f3 > f2));                                              \
  REQUIRE((v3 > v1 and v3 > v2));                                              \
  REQUIRE((f4 >= f1 and f4 >= f2));                                            \
  REQUIRE((v4 >= v1 and v4 >= v2));                                            \
  REQUIRE_FALSE(f3 < f1);                                                      \
  REQUIRE_FALSE(v3 < v1);                                                      \
  auto hash1 = hash(f1);                                                       \
  auto hash2 = hash(f2);                                                       \
  auto hash3 = hash(f3);                                                       \
  REQUIRE(hash1 == hash2);                                                     \
  REQUIRE_FALSE(hash1 == hash3);                                               \
  LOG(INFO) << f1

  SECTION("Float2") {
    auto f1 = Var(10.0, 2.0);
    std::vector<double> v1{10.0, 2.0};
    auto f2 = Var(10.0, 2.0);
    std::vector<double> v2{10.0, 2.0};
    auto f3 = Var(22.0, 2.0);
    std::vector<double> v3{22.0, 2.0};
    auto f4 = Var(22.0, 2.0);
    std::vector<double> v4{22.0, 2.0};
    auto i1 = Var(10);
    REQUIRE(f1.valueType == CBType::Float2);
    CBVECTOR_TESTS;
  }

  SECTION("Float3") {
    auto f1 = Var(10.0, 2.0, 3.0);
    std::vector<float> v1{10.0, 2.0, 3.0};
    auto f2 = Var(10.0, 2.0, 3.0);
    std::vector<float> v2{10.0, 2.0, 3.0};
    auto f3 = Var(22.0, 2.0, 1.0);
    std::vector<float> v3{22.0, 2.0, 1.0};
    auto f4 = Var(22.0, 2.0, 4.0);
    std::vector<float> v4{22.0, 2.0, 4.0};
    auto i1 = Var(10);
    REQUIRE(f1.valueType == CBType::Float3);
    CBVECTOR_TESTS;
  }

  SECTION("Float4") {
    auto f1 = Var(10.0, 2.0, 3.0, 3.0);
    std::vector<float> v1{10.0, 2.0, 3.0, 3.0};
    auto f2 = Var(10.0, 2.0, 3.0, 3.0);
    std::vector<float> v2{10.0, 2.0, 3.0, 3.0};
    auto f3 = Var(22.0, 2.0, 1.0, 3.0);
    std::vector<float> v3{22.0, 2.0, 1.0, 3.0};
    auto f4 = Var(22.0, 2.0, 4.0, 3.0);
    std::vector<float> v4{22.0, 2.0, 4.0, 3.0};
    auto i1 = Var(10);
    REQUIRE(f1.valueType == CBType::Float4);
    CBVECTOR_TESTS;
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
    std::vector<int64_t> v1{10, 2};
    auto f2 = Var(10, 2);
    std::vector<int64_t> v2{10, 2};
    auto f3 = Var(22, 2);
    std::vector<int64_t> v3{22, 2};
    auto f4 = Var(22, 2);
    std::vector<int64_t> v4{22, 2};
    auto i1 = Var(10.0);
    REQUIRE(f1.valueType == CBType::Int2);
    CBVECTOR_TESTS;
  }

  SECTION("Int3") {
    auto f1 = Var(10, 2, 3);
    std::vector<int32_t> v1{10, 2, 3};
    auto f2 = Var(10, 2, 3);
    std::vector<int32_t> v2{10, 2, 3};
    auto f3 = Var(22, 2, 1);
    std::vector<int32_t> v3{22, 2, 1};
    auto f4 = Var(22, 2, 4);
    std::vector<int32_t> v4{22, 2, 4};
    auto i1 = Var(10);
    REQUIRE(f1.valueType == CBType::Int3);
    CBVECTOR_TESTS;
  }

  SECTION("Int4") {
    auto f1 = Var(10, 2, 3, 0);
    std::vector<int32_t> v1{10, 2, 3, 0};
    auto f2 = Var(10, 2, 3, 0);
    std::vector<int32_t> v2{10, 2, 3, 0};
    auto f3 = Var(22, 2, 1, 0);
    std::vector<int32_t> v3{22, 2, 1, 0};
    auto f4 = Var(22, 2, 4, 0);
    std::vector<int32_t> v4{22, 2, 4, 0};
    auto i1 = Var(10);
    REQUIRE(f1.valueType == CBType::Int4);
    CBVECTOR_TESTS;
  }

  SECTION("Int8") {
    auto f1 = Var(10, 2, 3, 0, 1, 2, 3, 4);
    std::vector<int16_t> v1{10, 2, 3, 0, 1, 2, 3, 4};
    auto f2 = Var(10, 2, 3, 0, 1, 2, 3, 4);
    std::vector<int16_t> v2{10, 2, 3, 0, 1, 2, 3, 4};
    auto f3 = Var(22, 2, 1, 0, 1, 2, 3, 4);
    std::vector<int16_t> v3{22, 2, 1, 0, 1, 2, 3, 4};
    auto f4 = Var(22, 2, 4, 0, 1, 2, 3, 4);
    std::vector<int16_t> v4{22, 2, 4, 0, 1, 2, 3, 4};
    auto i1 = Var(10);
    REQUIRE(f1.valueType == CBType::Int8);
    CBVECTOR_TESTS;
  }

  SECTION("Int16") {
    auto f1 = Var(10, 2, 3, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2);
    std::vector<int8_t> v1{10, 2, 3, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2};
    auto f2 = Var(10, 2, 3, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2);
    std::vector<int8_t> v2{10, 2, 3, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2};
    auto f3 = Var(22, 2, 1, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2);
    std::vector<int8_t> v3{22, 2, 1, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2};
    auto f4 = Var(22, 2, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2);
    std::vector<int8_t> v4{22, 2, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2};
    auto i1 = Var(10);
    REQUIRE(f1.valueType == CBType::Int16);
    CBVECTOR_TESTS;
  }

#undef CBVECTOR_TESTS

  SECTION("Color") {
    auto c1 = Var(CBColor{20, 20, 20, 255});
    auto c2 = Var(CBColor{20, 20, 20, 255});
    auto c3 = Var(CBColor{20, 20, 20, 0});
    REQUIRE(c1 == c2);
    REQUIRE(c2 >= c1);
    REQUIRE(c1 != c3);
    REQUIRE(c1 > c3);
    REQUIRE(c1 >= c3);
    REQUIRE(c3 < c1);
    REQUIRE(c3 <= c1);
    auto hash1 = hash(c1);
    auto hash2 = hash(c2);
    auto hash3 = hash(c3);
    REQUIRE(hash1 == hash2);
    REQUIRE_FALSE(hash1 == hash3);
    LOG(INFO) << c1;
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

  SECTION("Bytes") {
    std::random_device rd;
    std::uniform_int_distribution<uint8_t> dist(0, 0x07);

    std::vector<uint8_t> data1(1024);
    for (auto &b : data1) {
      b = dist(rd);
    }
    Var v1{&data1.front(), uint32_t(data1.size())};

    auto data2 = data1;
    Var v2{&data2.front(), uint32_t(data2.size())};

    std::vector<uint8_t> data3(1024);
    for (auto &b : data3) {
      b = dist(rd);
    }
    Var v3{&data3.front(), uint32_t(data3.size())};

    std::vector<uint8_t> data4(1050);
    for (auto &b : data4) {
      b = dist(rd);
    }
    Var v4{&data4.front(), uint32_t(data4.size())};

    std::vector<uint8_t> data5(200);
    for (auto &b : data5) {
      b = dist(rd);
    }
    Var v5{&data5.front(), uint32_t(data5.size())};

    REQUIRE((&data1[0]) != (&data2[0]));
    REQUIRE(data1.size() == 1024);

    REQUIRE(v1.valueType == CBType::Bytes);

    REQUIRE(data1 == data2);
    REQUIRE(v1 == v2);

    REQUIRE(data1 != data3);
    REQUIRE(v1 != v3);

    REQUIRE(data1 != data3);
    REQUIRE(v1 != v3);

    REQUIRE(data1 >= data2);
    REQUIRE(v1 >= v2);

    REQUIRE_FALSE(data1 < data2);
    REQUIRE_FALSE(v1 < v2);

    REQUIRE((v1 > v5) == (data1 > data5));
    REQUIRE((v1 > v4) == (data1 > data4));

    REQUIRE(v1 == v1);
    REQUIRE(v1 <= v1);
    REQUIRE_FALSE(v1 < v1);
  }
}

TEST_CASE("CBSet") {
  CBSet x;
  x.insert(Var(10));
  x.emplace(Var("Hello Set"));
  REQUIRE(x.size() == 2);
  REQUIRE(x.count(Var(10)) == 1);
  x.erase(Var(10));
  REQUIRE(x.count(Var(10)) == 0);
  REQUIRE(x.count(Var("Hello Set")) == 1);
  x.erase(Var("Hello Set"));
  REQUIRE(x.count(Var("Hello Set")) == 0);
}
