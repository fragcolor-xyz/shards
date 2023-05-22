#include "./context.hpp"
#include "./data.hpp"
#include <catch2/catch_all.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

using namespace gfx;

const float threshold = 1.0f;

TEST_CASE("Self Test 0", "[Comparison]") {
  TestData testData(TestPlatformId::Default);

  TestFrame a = testData.loadFrame("compare_0a");
  TestFrame b = testData.loadFrame("compare_0b");
  auto comparison = a.compare(b);

  // Visualize
  // TestFrame diff = generateComparisonDiffTestFrame(comparison, a.getSize());
  // testData.writeRejectedFrame("compare_0", diff);

  // Image: different colores over wide area
  // Expect: bad
  CHECK(comparison.getDeltaOverTotal() > threshold);
}

TEST_CASE("Self Test 1", "[Comparison]") {
  TestData testData(TestPlatformId::Default);

  TestFrame a = testData.loadFrame("compare_1a");
  TestFrame b = testData.loadFrame("compare_1b");
  auto comparison = a.compare(b);

  // Visualize
  // TestFrame diff = generateComparisonDiffTestFrame(comparison, a.getSize());
  // testData.writeRejectedFrame("compare_1", diff);

  // Image: result shifted by (1,1) pixels
  // Expect: good
  CHECK(comparison.getDeltaOverTotal() < threshold);
}

TEST_CASE("Self Test 2", "[Comparison]") {
  TestData testData(TestPlatformId::Default);

  TestFrame a = testData.loadFrame("compare_2a");
  TestFrame b = testData.loadFrame("compare_2b");
  auto comparison = a.compare(b);

  // Visualize
  // TestFrame diff = generateComparisonDiffTestFrame(comparison, a.getSize());
  // testData.writeRejectedFrame("compare_2", diff);

  // Image: slightly different colors and patterns in various spots
  // Expect: bad
  CHECK(comparison.getDeltaOverTotal() > threshold);
}

TEST_CASE("Self Test 3", "[Comparison]") {
  TestData testData(TestPlatformId::Default);

  TestFrame a = testData.loadFrame("compare_3a");
  TestFrame b = testData.loadFrame("compare_3b");
  auto comparison = a.compare(b);

  // Visualize
  // TestFrame diff = generateComparisonDiffTestFrame(comparison, a.getSize());
  // testData.writeRejectedFrame("compare_3", diff);

  // Image: Missing one red outer line
  // Expect: bad
  CHECK(comparison.getDeltaOverTotal() > threshold);
}
