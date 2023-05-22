#ifndef GFX_TESTS_DATA
#define GFX_TESTS_DATA

#include "./platform_id.hpp"
#include <gfx/filesystem.hpp>
#include <gfx/linalg.hpp>
#include <stdint.h>
#include <vector>

namespace gfx {
struct Texture;
struct SingleFrameCapture;

enum class TestFrameFormat {
  BGRA8,
  RGBA8,
  R32F,
};

struct Comparison {
  std::vector<int16_t> delta;
  size_t totalNegDelta{};
  size_t totalPosDelta{};
  size_t numDeltaPixels{};

  // Average difference divided by changed pixels
  double getDeltaOverChanged() const { return (double)std::max(totalNegDelta, totalPosDelta) / double(numDeltaPixels); }

  // Average difference divided by total pixels
  double getDeltaOverTotal() const { return (double)std::max(totalNegDelta, totalPosDelta) / double(delta.size()); }
};

struct TestFrame {
  typedef uint32_t pixel_t;

private:
  std::vector<pixel_t> pixels;
  int2 size;

public:
  TestFrame() = default;
  TestFrame(const uint8_t *imageData, int2 size, TestFrameFormat format, uint32_t pitch, bool yflip);
  void set(const uint8_t *imageData, int2 size, TestFrameFormat format, uint32_t pitch, bool yflip);
  Comparison compare(const TestFrame &other) const;

  const std::vector<pixel_t> &getPixels() const { return pixels; }
  int2 getSize() const { return size; }
};

TestFrame generateComparisonDiffTestFrame(const Comparison &comparison, int2 originalSize);

struct TestData {
private:
  fs::path basePath;
  bool overwriteAll = false;

public:
  TestData(const TestPlatformId &testPlatformId);
  bool checkFrame(const char *id, const TestFrame &frame, float tolerance = 0.f, Comparison *comparison = nullptr);

  TestFrame loadFrame(const char *id);
  void writeRejectedFrame(const char *id, const TestFrame &frame);

private:
  bool loadFrameInternal(TestFrame &frame, const char *filePath);
  void storeFrameInternal(const TestFrame &frame, const char *filePath);
  void writeRejectionDetails(const char *id, const TestFrame &frame, const TestFrame &referenceFrame,
                             const Comparison &comparison, float tolerance);
  fs::path getRejectedBasePath(const char *id);
};
} // namespace gfx

#endif // GFX_TESTS_DATA
