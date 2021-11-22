#pragma once
#include "bgfx/bgfx.h"
#include <bx/filepath.h>
#include <gfx/linalg.hpp>
#include <gfx/test_platform_id.hpp>
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

struct TestFrame {
	typedef uint32_t pixel_t;

private:
	std::vector<pixel_t> pixels;
	int2 size;

public:
	TestFrame() = default;
	TestFrame(const uint8_t *imageData, int2 size, TestFrameFormat format, uint32_t pitch, bool yflip);
	TestFrame(const SingleFrameCapture &capture);
	void set(const uint8_t *imageData, int2 size, TestFrameFormat format, uint32_t pitch, bool yflip);
	bool compare(const TestFrame &other, float tolerance = 0.f) const;

	const std::vector<pixel_t> &getPixels() const { return pixels; }
	int2 getSize() const { return size; }
};

struct TestData {
private:
	TestPlatformId testPlatformId;
	bx::FilePath basePath;
	bool overwriteAll = false;

public:
	TestData() = default;
	TestData(const TestPlatformId &testPlatformId);
	bool checkFrame(const char *id, const TestFrame &frame, float tolerance = 0.f);

private:
	bool loadFrame(TestFrame &frame, const char *filePath);
	void storeFrame(const TestFrame &frame, const char *filePath);
};
} // namespace gfx
