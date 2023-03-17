#ifndef GFX_VIEW
#define GFX_VIEW

#include "math.hpp"
#include "types.hpp"
#include "unique_id.hpp"

#include <optional>
#include <variant>

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

namespace gfx {
enum class FovDirection {
  Horizontal,
  Vertical,
};

struct ViewPerspectiveProjection {
  float fov = degToRad(45.0f);
  FovDirection fovType = FovDirection::Horizontal;
  float near = 0.1f;
  float far = 800.0f;
};

enum class OrthographicSizeType {
  Horizontal,
  Vertical,
  PixelScale,
};
struct ViewOrthographicProjection {
  float size;
  OrthographicSizeType sizeType = OrthographicSizeType::Horizontal;
  float near = 0.0f;
  float far = 1000.0f;
};

extern UniqueIdGenerator viewIdGenerator;

struct alignas(16) View {
public:
  UniqueId id = viewIdGenerator.getNext();

  float4x4 view;
  std::variant<std::monostate, ViewPerspectiveProjection, ViewOrthographicProjection, float4x4> proj;

public:
  View();
  View(const View &) = delete;

  float4x4 getProjectionMatrix(const float2 &viewSize) const;
  UniqueId getId() const { return id; }
};

} // namespace gfx

#endif // GFX_VIEW
