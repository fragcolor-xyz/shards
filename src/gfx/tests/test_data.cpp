#include "test_data.hpp"
#include "gfx/tests/test_data.hpp"
#include "gfx/types.hpp"
#include <bgfx/bgfx.h>
#include <bx/file.h>
#include <bx/filepath.h>
#include <cassert>
#include <gfx/capture.hpp>
#include <gfx/paths.hpp>
#include <gfx/types.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

namespace gfx {
TestFrame::TestFrame(uint32_t *imageData, int2 size, uint32_t pitch, bool yflip) { set(imageData, size, pitch, yflip); }

TestFrame::TestFrame(const SingleFrameCapture &capture) {
	assert(capture.format.format == bgfx::TextureFormat::BGRA8);
	assert(capture.format.size.x > 0 && capture.format.size.y > 0);

	uint32_t *srcData = (uint32_t *)capture.data.data();
	set(srcData, capture.format.size, capture.format.pitch, capture.format.yflip);

	// ARGB to ABGR
	uint32_t *pixelData = pixels.data();
	size_t numPixels = size.x * size.y;
	for (size_t p = 0; p < numPixels; p++) {
		Color color = colorFromARGB(pixelData[p]);
		pixelData[p] = (uint32_t(color.x) << 0) | (uint32_t(color.y) << 8) | (uint32_t(color.z) << 16) | (uint32_t(color.w) << 24);
	}
}

void TestFrame::set(uint32_t *imageData, int2 size, uint32_t pitch, bool yflip) {
	this->size = size;
	size_t numPixels = size.x * size.y;

	uint8_t *srcData = (uint8_t *)imageData;

	pixels.resize(numPixels);
	for (size_t _y = 0; _y < size_t(size.y); _y++) {
		size_t srcY = _y;
		size_t dstY = yflip ? (size.y - 1 - _y) : _y;

		memcpy(&pixels[dstY * size.x], &srcData[srcY * pitch], sizeof(pixel_t) * size.x);
	}
}

bool TestFrame::compare(const TestFrame &other, float tolerance) const {
	uint8_t byteTolerance = uint8_t(255.0f * tolerance);

	assert(size == other.size);

	size_t numPixels = size.x * size.y;
	for (size_t pi = 0; pi < numPixels; pi++) {
		int32_t delta = int32_t(other.pixels[pi]) - int32_t(pixels[pi]);
		if (std::abs(delta) > byteTolerance)
			return false;
	}
	return true;
}

TestData::TestData(const TestPlatformId &testPlatformId) : testPlatformId(testPlatformId) {
	basePath.set(resolveDataPath(GFX_TEST_DATA_PATH).c_str());
	basePath.join(std::string(testPlatformId).c_str());
	bx::makeAll(basePath);
}

bool TestData::checkFrame(const char *id, const TestFrame &frame, float tolerance) {
	std::string filename = std::string(id) + ".png";

	bx::FilePath filePath = basePath;
	filePath.join(filename.c_str());

	// stbi_write_png(
	TestFrame referenceFrame;
	if (loadFrame(referenceFrame, filePath.getCPtr())) {
		return referenceFrame.compare(frame, tolerance);
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

	frame.set((uint32_t *)data, size, size.x * sizeof(TestFrame::pixel_t), false);

	return true;
}

void TestData::storeFrame(const TestFrame &frame, const char *filePath) {
	int2 size = frame.getSize();
	assert(size.x > 0 && size.y > 0);

	const std::vector<TestFrame::pixel_t> &pixels = frame.getPixels();
	stbi_write_png(filePath, size.x, size.y, 4, pixels.data(), sizeof(TestFrame::pixel_t) * size.x);
}

} // namespace gfx
