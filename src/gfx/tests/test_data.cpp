#include "test_data.hpp"
#include "gfx/tests/test_data.hpp"
#include "utils.hpp"
#include <SDL_stdinc.h>
#include <bgfx/bgfx.h>
#include <bx/error.h>
#include <bx/file.h>
#include <bx/filepath.h>
#include <bx/readerwriter.h>
#include <cassert>
#include <gfx/capture.hpp>
#include <gfx/paths.hpp>
#include <gfx/texture.hpp>
#include <gfx/types.hpp>
#include <gfx/utils.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
// #define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#pragma GCC diagnostic pop

namespace gfx {
TestFrame::TestFrame(const uint8_t *imageData, int2 size, TestFrameFormat format, uint32_t pitch, bool yflip) { set(imageData, size, format, pitch, yflip); }

TestFrame::TestFrame(const SingleFrameCapture &capture) {
	assert(capture.format.size.x > 0 && capture.format.size.y > 0);

	TestFrameFormat testFrameFormat;
	switch (capture.format.format) {
	case bgfx::TextureFormat::BGRA8:
		testFrameFormat = TestFrameFormat::BGRA8;
		break;
	case bgfx::TextureFormat::RGBA8:
		testFrameFormat = TestFrameFormat::RGBA8;
		break;
	case bgfx::TextureFormat::R32F:
		testFrameFormat = TestFrameFormat::R32F;
		break;
	default:
		assert(false && "Unsupported format");
	}

	set(capture.data.data(), capture.format.size, testFrameFormat, capture.format.pitch, capture.format.yflip);
}

template <typename TSrcFormat, typename TLambda> void copyToARGB8(uint32_t *dst, const uint8_t *src, int2 size, uint32_t pitch, bool yflip, TLambda &&convert) {
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
		assert(false && "Unsupported format");
	}
}

bool TestFrame::compare(const TestFrame &other, float tolerance, CompareRejection *rejection) const {
	uint8_t byteTolerance = uint8_t(255.0f * tolerance);

	assert(size == other.size);

	uint8_t *bytesA = (uint8_t *)pixels.data();
	uint8_t *bytesB = (uint8_t *)other.pixels.data();
	size_t numPixels = size.x * size.y;
	for (size_t pi = 0; pi < numPixels; pi++) {
		size_t pixelOffset = pi * 4;
		for (size_t b = 0; b < 4; b++) {
			int16_t pa(bytesA[pixelOffset + b]);
			int16_t pb(bytesB[pixelOffset + b]);
			int16_t delta = pb - pa;
			if (std::abs(delta) > byteTolerance) {
				if (rejection) {
					rejection->position.y = pi / size.x;
					rejection->position.x = pi - (rejection->position.y * size.x);
					rejection->component = b;
					rejection->a = uint8_t(pa);
					rejection->b = uint8_t(pb);
				}
				return false;
			}
		}
	}
	return true;
}

TestData::TestData(const TestPlatformId &testPlatformId) : testPlatformId(testPlatformId) {
	basePath.set(resolveDataPath(GFX_TEST_DATA_PATH).c_str());
	basePath.join(std::string(testPlatformId).c_str());
	bx::makeAll(basePath);

	if (gfx::getEnvFlag("GFX_OVERWRITE_TEST_DATA")) {
		overwriteAll = true;
	}
}

bool TestData::checkFrame(const char *id, const TestFrame &frame, float tolerance, CompareRejection *rejection) {
	std::string filename = std::string(id) + ".png";

	bx::FilePath filePath = basePath;
	filePath.join(filename.c_str());

	TestFrame referenceFrame;
	if (!overwriteAll && loadFrame(referenceFrame, filePath.getCPtr())) {
		return referenceFrame.compare(frame, tolerance, rejection);
	} else {
		// Write reference
		storeFrame(frame, filePath.getCPtr());
		return true;
	}
}

bool TestData::loadFrame(TestFrame &frame, const char *filePath) {
	int2 size;
	int32_t numChannels;
	uint8_t *data = stbi_load(filePath, &size.x, &size.y, &numChannels, 4);
	if (numChannels != 4) {
		stbi_image_free(data);
		return false;
	}

	frame.set(data, size, TestFrameFormat::RGBA8, size.x * sizeof(TestFrame::pixel_t), false);

	return true;
}

void TestData::storeFrame(const TestFrame &frame, const char *filePath) {
	int2 size = frame.getSize();
	assert(size.x > 0 && size.y > 0);

	spdlog::info("Writing frame data to {}", filePath);
	const std::vector<TestFrame::pixel_t> &pixels = frame.getPixels();
	stbi_write_png(filePath, size.x, size.y, 4, pixels.data(), sizeof(TestFrame::pixel_t) * size.x);

#ifdef __EMSCRIPTEN__
	emrun_copyFileToOutput(filePath);
#endif
}

} // namespace gfx
