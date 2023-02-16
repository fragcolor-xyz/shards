#include "render_target.hpp"
#include "fwd.hpp"
#include <spdlog/fmt/fmt.h>

namespace gfx {
void RenderTarget::configure(const char *name, WGPUTextureFormat format) {
  auto attachment = Texture::makeRenderAttachment(format, fmt::format("{}/{}", label, name));
  attachments.insert_or_assign(name, attachment);
}

int2 RenderTarget::resizeConditional(int2 mainOutputSize) {
  std::visit(
      [&](auto &&arg) {
        computedSize = arg.apply(mainOutputSize);
        for (auto &it : attachments)
          it.second.texture->initWithResolution(computedSize);
      },
      size);
  return computedSize;
}

int2 RenderTarget::getSize() const { return computedSize; }
int2 RenderTarget::computeSize(int2 mainOutputSize) const {
  int2 result{};
  std::visit([&](auto &&arg) { result = arg.apply(mainOutputSize); }, size);
  return result;
}

const TextureSubResource &RenderTarget::getAttachment(const std::string &name) const {
  auto it = attachments.find(name);
  if (it != attachments.end())
    return it->second;

  static TextureSubResource None{TexturePtr()};
  return None;
}
} // namespace gfx
