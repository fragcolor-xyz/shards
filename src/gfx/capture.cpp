#include "capture.hpp"

namespace gfx {
void SingleFrameCapture::frame(const CaptureFormat &format, const void *inData,
							   uint32_t size) {
  this->format = format;
  data.resize(size);
  memcpy(data.data(), inData, size);
  frameCounter += 1; 
}
} // namespace gfx