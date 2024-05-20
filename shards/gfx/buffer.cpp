#include "buffer.hpp"
#include "context.hpp"
#include "../core/assert.hpp"
#include <spdlog/spdlog.h>

namespace gfx {
UniqueId Buffer::getNextId() {
  static UniqueIdGenerator gen(UniqueIdTag::Buffer);
  return gen.getNext();
}

Buffer &Buffer::setAddressSpaceUsage(shader::AddressSpace addressSpaceUsage) {
  if (this->addressSpaceUsage != addressSpaceUsage) {
    this->addressSpaceUsage = addressSpaceUsage;
    this->version++;
  }
  return *this;
}
Buffer &Buffer::setLabel(const std::string &label) {
  if (this->label != label) {
    this->label = label;
    this->version++;
  }
  return *this;
}

Buffer &Buffer::update(ImmutableSharedBuffer data) {
  this->data = data;
  this->version++;
  return *this;
}

BufferPtr Buffer::clone() const {
  BufferPtr result = cloneSelfWithId(this, getNextId());
  return result;
}

void Buffer::initContextData(Context &context, BufferContextData &contextData) {}

void Buffer::updateContextData(Context &context, BufferContextData &contextData) {
  ImmutableSharedBuffer data = this->data;
  std::string labelCopy = this->label;
  shader::AddressSpace usage = this->addressSpaceUsage;

  WGPUBufferDescriptor desc = {};
  if (usage == shader::AddressSpace::Uniform) {
    desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
  } else {
    desc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
  }
  if (data.getLength() > contextData.bufferLength || contextData.currentUsage != desc.usage) {
    desc.label = labelCopy.c_str();
    desc.size = data.getLength();
    contextData.buffer.reset(wgpuDeviceCreateBuffer(context.wgpuDevice, &desc));
    contextData.bufferLength = desc.size;
  }

  if (data.getLength() > 0) {
    shassert(contextData.buffer);
    wgpuQueueWriteBuffer(context.wgpuQueue, contextData.buffer, 0, data.getData(), data.getLength());
  }
}
} // namespace gfx