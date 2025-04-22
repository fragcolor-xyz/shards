#include "window.hpp"
#include "renderer.hpp"

namespace gfx {
template <typename T>
concept AutoObjectType = requires(T t) {
  { T::VariableName } -> std::convertible_to<const char *>;
  { T::Type } -> std::convertible_to<const SHTypeInfo &>;
};

template <AutoObjectType T> void registerAutoObjectType(bool threadSafe = false) {
  shards::registerObjectType(T::Type.object.vendorId, T::Type.object.typeId,
                             SHObjectInfo{
                                 .name = T::VariableName,
                                 .isThreadSafe = threadSafe,
                             });
}

void registerObjectTypes() {
  registerAutoObjectType<shards::WindowContext>();
  registerAutoObjectType<GraphicsRendererContext>();
  registerAutoObjectType<GraphicsContext>();
}
} // namespace gfx