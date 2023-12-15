#include <catch2/catch_all.hpp>
#include <shards/fast_string/fast_string.hpp>
#include <unordered_map>

using String = shards::fast_string::FastString;
TEST_CASE("Fast string", "[FastString]") {
  shards::fast_string::init();

  String a = "hello";
  CHECK(!a.empty());

  String empty;
  CHECK(empty.empty());

  String b = "world";
  CHECK(!b.empty());

  CHECK(a != empty);
  CHECK(a != b);
  CHECK(a != b);

  CHECK(a == a);
  CHECK(b == b);
  CHECK(empty == empty);

  CHECK((a < b) != (b < a));
  CHECK((a < empty) != (empty < a));

  CHECK(std::string(a) == "hello");
  CHECK(std::string(b) == "world");

  // Check that we can hash
  std::unordered_map<String, std::string> hashMap;
  hashMap[a] = "hello";
  hashMap[b] = "world";
  hashMap[empty] = "";
  CHECK(hashMap.size() == 3);
  CHECK(hashMap[a] == "hello");
  CHECK(hashMap[b] == "world");
  CHECK(hashMap[empty] == "");

  // update this value
  hashMap[""] = "empty";
  CHECK(hashMap.size() == 3);
  CHECK(hashMap[empty] == "empty");
}