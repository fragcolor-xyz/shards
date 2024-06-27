#include "./data.hpp"
#include <SDL3/SDL_stdinc.h>
#include <cassert>
#include <fstream>
#include <gfx/paths.hpp>
#include <gfx/types.hpp>
#include <gfx/utils.hpp>
#include <spdlog/spdlog.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#pragma GCC diagnostic pop

namespace gfx {
TestFrame::TestFrame(const uint8_t *imageData, int2 size, TestFrameFormat format, uint32_t pitch, bool yflip) {
  set(imageData, size, format, pitch, yflip);
}

template <typename TSrcFormat, typename TLambda>
void copyToARGB8(uint32_t *dst, const uint8_t *src, int2 size, uint32_t pitch, bool yflip, TLambda &&convert) {
  for (size_t _y = 0; _y < size_t(size.y); _y++) {
    size_t srcY = _y;
    size_t dstY = yflip ? (size.y - 1 - _y) : _y;

    uint32_t *dstLine = &dst[dstY * size.x];
    TSrcFormat *srcLine = (TSrcFormat *)&src[srcY * pitch];

    for (size_t x = 0; x < size_t(size.x); x++) {
      dstLine[x] = convert(srcLine[x]);
    }
  }
}

void TestFrame::set(const uint8_t *imageData, int2 size, TestFrameFormat format, uint32_t pitch, bool yflip) {
  this->size = size;

  size_t numPixels = size.x * size.y;
  pixels.resize(numPixels);

  uint8_t *srcData = (uint8_t *)imageData;

  // Internal layout (bit offset)
  // 24 16 8  0
  // A  B  G  R
  if (format == TestFrameFormat::BGRA8) {
    copyToARGB8<uint32_t>(pixels.data(), srcData, size, pitch, yflip, [](const uint32_t &srcColor) -> uint32_t {
      const uint8_t *color = (uint8_t *)&srcColor;
      return (uint32_t(color[2]) << 0) | (uint32_t(color[1]) << 8) | (uint32_t(color[0]) << 16) | (uint32_t(color[3]) << 24);
    });
  } else if (format == TestFrameFormat::RGBA8) {
    copyToARGB8<uint32_t>(pixels.data(), srcData, size, pitch, yflip, [](const uint32_t &srcColor) -> uint32_t {
      const uint8_t *color = (uint8_t *)&srcColor;
      return (uint32_t(color[0]) << 0) | (uint32_t(color[1]) << 8) | (uint32_t(color[2]) << 16) | (uint32_t(color[3]) << 24);
    });
  } else if (format == TestFrameFormat::R32F) {
    copyToARGB8<float>(pixels.data(), srcData, size, pitch, yflip, [](const float &srcColor) -> uint32_t {
      uint8_t r = uint8_t(srcColor * 255.0f);
      return (uint32_t(r) << 0) | (uint32_t(0) << 8) | (uint32_t(0) << 16) | (uint32_t(255) << 24);
    });
  } else {
    shassert(false && "Unsupported format");
  }
}

Comparison TestFrame::compare(const TestFrame &other) const {
  Comparison result;

  if (size != other.size) {
    throw std::runtime_error("Image sizes do not match");
  }

  result.delta.resize(pixels.size());
  memset(result.delta.data(), 0, result.delta.size() * sizeof(decltype(result.delta)::value_type));

  uint8_t *bytesA = (uint8_t *)pixels.data();
  uint8_t *bytesB = (uint8_t *)other.pixels.data();
  size_t numPixels = size.x * size.y;
  for (size_t pi = 0; pi < numPixels; pi++) {
    size_t pixelOffset = pi * 4;
    int16_t &pixelDelta = result.delta[pi];
    for (size_t compIndex = 0; compIndex < 4; compIndex++) {
      int16_t pa(bytesA[pixelOffset + compIndex]);
      int16_t pb(bytesB[pixelOffset + compIndex]);
      int16_t componentDelta = pa - pb;

      // Define pixel delta as the largest differing component
      if (componentDelta != 0) {
        if (std::abs(componentDelta) > std::abs(pixelDelta))
          pixelDelta = componentDelta;
      }
    }

    if (pixelDelta) {
      result.totalPosDelta += size_t(std::max<int16_t>(0, pixelDelta));
      result.totalNegDelta += size_t(std::max<int16_t>(0, -pixelDelta));
      result.numDeltaPixels++;
    }
  }

  return result;
}

TestFrame generateComparisonDiffTestFrame(const Comparison &comparison, int2 originalSize) {
  std::vector<TestFrame::pixel_t> resultPixels;

  size_t numPixels = comparison.delta.size();
  resultPixels.resize(numPixels);
  for (size_t pi = 0; pi < numPixels; pi++) {
    int16_t delta = comparison.delta[pi];
    TestFrame::pixel_t pixel = 0xFF000000;
    pixel |= (std::max<int16_t>(0, delta) & 0xFF) << 8;  // G
    pixel |= (std::max<int16_t>(0, -delta) & 0xFF) << 0; // R
    resultPixels[pi] = pixel;
  }

  return TestFrame((uint8_t *)resultPixels.data(), originalSize, TestFrameFormat::RGBA8,
                   sizeof(TestFrame::pixel_t) * originalSize.x, false);
}

TestData::TestData(const TestPlatformId &testPlatformId) {
  basePath = resolveDataPath(GFX_TEST_DATA_PATH);
  basePath /= std::string(testPlatformId);
  fs::create_directories(basePath);

  if (gfx::getEnvFlag("GFX_OVERWRITE_TEST_DATA")) {
    overwriteAll = true;
  }
}

bool TestData::checkFrame(const char *id, const TestFrame &frame, float tolerance, Comparison *outComparison) {
  std::string filename = std::string(id) + ".png";

  fs::path filePath = basePath;
  filePath /= filename;

  Comparison localComparison{};
  outComparison = outComparison ? outComparison : &localComparison;

  TestFrame referenceFrame;
  if (!overwriteAll && loadFrameInternal(referenceFrame, filePath.string().c_str())) {
    *outComparison = referenceFrame.compare(frame);
    if (outComparison->getDeltaOverTotal() >= tolerance) {
      writeRejectionDetails(id, frame, referenceFrame, *outComparison, tolerance);
      return false;
    }
    return true;
  } else {
    // Write reference
    storeFrameInternal(frame, filePath.string().c_str());
    return true;
  }
}

TestFrame TestData::loadFrame(const char *id) {
  std::string filename = std::string(id) + ".png";

  fs::path filePath = basePath;
  filePath /= filename;

  TestFrame frame;
  if (!loadFrameInternal(frame, filePath.string().c_str()))
    throw std::runtime_error(fmt::format("Failed to load frame {} ({})", id, filePath.string()));

  return frame;
}

fs::path TestData::getRejectedBasePath(const char *id) {
  fs::path rejectedDirPath = basePath / "rejected";
  fs::create_directories(rejectedDirPath);

  fs::path baseFilePath = rejectedDirPath / id;
  return baseFilePath;
}

void TestData::writeRejectedFrame(const char *id, const TestFrame &frame) {
  fs::path baseFilePath = getRejectedBasePath(id);
  std::string imagePath = baseFilePath.string() + ".png";

  storeFrameInternal(frame, imagePath.c_str());
}

void TestData::writeRejectionDetails(const char *id, const TestFrame &frame, const TestFrame &referenceFrame,
                                     const Comparison &comparison, float tolerance) {
  using std::endl;
  using std::ofstream;

  fs::path baseFilePath = getRejectedBasePath(id);
  std::string imagePath = baseFilePath.string() + ".png";
  std::string deltaImagePath = baseFilePath.string() + ".delta.png";
  std::string logPath = baseFilePath.string() + ".log";

  ofstream outLog;
  outLog.open(logPath);
  outLog << fmt::format("Comparison failed") << endl;
  outLog << fmt::format("Different pixels: {}", comparison.numDeltaPixels) << endl;
  outLog << fmt::format("Changed avg. delta: {}", comparison.getDeltaOverChanged()) << endl;
  outLog << fmt::format("Total avg. delta: {}", comparison.getDeltaOverTotal()) << endl;
  outLog << fmt::format("Tolerance: {}", tolerance) << endl;
  outLog.close();

  auto deltaFrame = generateComparisonDiffTestFrame(comparison, frame.getSize());
  storeFrameInternal(frame, imagePath.c_str());
  storeFrameInternal(deltaFrame, deltaImagePath.c_str());
}

bool TestData::loadFrameInternal(TestFrame &frame, const char *filePath) {
  int2 size;
  int32_t numChannels;
  stbi_set_flip_vertically_on_load_thread(0);
  uint8_t *data = stbi_load(filePath, &size.x, &size.y, &numChannels, 4);
  if (numChannels != 4) {
    stbi_image_free(data);
    return false;
  }

  frame.set(data, size, TestFrameFormat::RGBA8, size.x * sizeof(TestFrame::pixel_t), false);

  return true;
}

void TestData::storeFrameInternal(const TestFrame &frame, const char *filePath) {
  int2 size = frame.getSize();
  if (size.x <= 0 || size.y <= 0) {
    spdlog::warn("Can not write empty test frame");
    return;
  }

  SPDLOG_INFO("Writing frame data to {}", filePath);
  const std::vector<TestFrame::pixel_t> &pixels = frame.getPixels();
  stbi_write_png(filePath, size.x, size.y, 4, pixels.data(), sizeof(TestFrame::pixel_t) * size.x);

#ifdef __EMSCRIPTEN__
  // TODO(guusw): This needs to be implemented using emrun_file_dump
  // old version was still using bx
  // reference 08005aad738ae1c967363afbddec17308602497b:src/gfx/tests/utils.cpp
  // emrun_copyFileToOutput(filePath);
#endif
}

} // namespace gfx
