#pragma once
#include "linalg.hpp"
#include <bgfx/bgfx.h>
#include <memory>
#include <stdint.h>
#include <vector>

namespace gfx {
struct CaptureFormat {
  int2 size;
  uint32_t pitch;
  bgfx::TextureFormat::Enum format;
  bool yflip;
};

struct ICapture {
  virtual ~ICapture() = default;
  virtual void frame(const CaptureFormat &format, const void *data,
					 uint32_t size) = 0;
};

struct SingleFrameCapture : public ICapture {
  CaptureFormat format;
  std::vector<uint8_t> data;
  uint32_t frameCounter = ~0;

  void frame(const CaptureFormat &format, const void *data, uint32_t size);
  static std::shared_ptr<SingleFrameCapture> create() {
	return std::make_shared<SingleFrameCapture>();
  }
};
} // namespace gfx