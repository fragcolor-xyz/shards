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
#include "log.hpp"

namespace gfx {

/// <div rustbindgen opaque></div>
struct BufferContextData : public ContextData {
  WgpuHandle<WGPUBuffer> buffer;
  size_t bufferLength{};
  WGPUBufferUsage currentUsage{};

#if SH_GFX_CONTEXT_DATA_LABELS
  std::string label;
  std::string_view getLabel() const { return label; }
#endif

  BufferContextData() = default;
  BufferContextData(BufferContextData &&) = default;

  void init(std::string_view label) {
#if SH_GFX_CONTEXT_DATA_LABELS
    this->label = label;
#endif
#if SH_GFX_CONTEXT_DATA_LOG_LIFETIME
    SPDLOG_LOGGER_DEBUG(getContextDataLogger(), "Buffer {} data created", getLabel());
#endif
  }

#if SH_GFX_CONTEXT_DATA_LOG_LIFETIME
  ~BufferContextData() { SPDLOG_LOGGER_DEBUG(getContextDataLogger(), "Buffer {} data destroyed", getLabel()); }
#endif
};

/// <div rustbindgen opaque></div>
struct Buffer final {
  using ContextDataType = BufferContextData;

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
  Buffer &setLabel(const std::string &);
  std::string_view getLabel() const;

  // Updates buffer data with length in bytes
  Buffer &update(ImmutableSharedBuffer data);

  UniqueId getId() const { return id; }
  BufferPtr clone() const;

  void initContextData(Context &context, BufferContextData &contextData);
  void updateContextData(Context &context, BufferContextData &contextData);

protected:
  static UniqueId getNextId();
};

} // namespace gfx
#endif /* B473CE26_9C2F_45DE_83BF_A386A16F9EE7 */
