#ifndef B473CE26_9C2F_45DE_83BF_A386A16F9EE7
#define B473CE26_9C2F_45DE_83BF_A386A16F9EE7

#include "fwd.hpp"
#include "context_data.hpp"
#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "wgpu_handle.hpp"
#include "isb.hpp"
#include "unique_id.hpp"
#include "shader/types.hpp"

namespace gfx {

/// <div rustbindgen opaque></div>
struct BufferContextData : public ContextData {
  WgpuHandle<WGPUBuffer> buffer;
  size_t bufferLength{};
  size_t version{};

  ~BufferContextData() { releaseContextDataConditional(); }
  void releaseContextData() override {
    buffer.release();
  }
};

/// <div rustbindgen opaque></div>
struct Buffer final : public TWithContextData<BufferContextData> {
private:
  UniqueId id = getNextId();
  ImmutableSharedBuffer data;
  std::string label;
  shader::AddressSpace addressSpaceUsage = shader::AddressSpace::Uniform;
  size_t version{};

  friend struct gfx::UpdateUniqueId<Buffer>;

public:
  const ImmutableSharedBuffer &getData() const { return data; }

  // This increments every time the buffer is updated
  size_t getVersion() const { return version; }

  Buffer &setAddressSpaceUsage(shader::AddressSpace addressSpaceUsage);
  Buffer& setLabel(const std::string&);

  // Updates buffer data with length in bytes
  Buffer& update(ImmutableSharedBuffer data);

  UniqueId getId() const { return id; }
  BufferPtr clone() const;

protected:
  void initContextData(Context &context, BufferContextData &contextData);
  void updateContextData(Context &context, BufferContextData &contextData);

  static UniqueId getNextId();
};

} // namespace gfx
#endif /* B473CE26_9C2F_45DE_83BF_A386A16F9EE7 */
