#pragma once
#include <bx/filepath.h>
#include <gfx/linalg.hpp>
#include <gfx/test_platform_id.hpp>
#include <stdint.h>
#include <vector>

namespace gfx {
struct SingleFrameCapture;
struct TestFrame {
	typedef uint32_t pixel_t;

private:
	std::vector<pixel_t> pixels;
	int2 size;

public:
	TestFrame() = default;
	TestFrame(uint32_t *imageData, int2 size, uint32_t pitch, bool yflip);
	TestFrame(const SingleFrameCapture &capture);
	void set(uint32_t *imageData, int2 size, uint32_t pitch, bool yflip);
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
