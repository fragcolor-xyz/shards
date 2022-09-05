#ifndef A1F9DAED_695B_4350_8992_7D7859A27481
#define A1F9DAED_695B_4350_8992_7D7859A27481

// Default gizmo implementations

#include "../fmt.hpp"
#include "../fwd.hpp"
#include "../linalg.hpp"
#include "gizmo_input.hpp"
#include "gizmo_math.hpp"
#include "shapes.hpp"
#include <spdlog/spdlog.h>

namespace gfx {
namespace gizmos {

struct IGizmo {
  virtual ~IGizmo() = default;
  virtual void update(InputContext &inputContext) = 0;
  virtual void render(InputContext &inputContext, GizmoRenderer &renderer) = 0;
};

// Combined context for gizmo rendering and input handling
// user is responsible for keeping gizmo data alive between begin/end
struct Context {
  GizmoRenderer renderer;
  InputContext input;

private:
  std::vector<IGizmo *> gizmos;

public:
  void begin(const InputState &inputState, ViewPtr view);
  void updateGizmo(IGizmo &gizmo);
  void end(DrawQueuePtr drawQueue);
};

inline constexpr Colorf axisColors[3] = {
    colorToFloat(colorFromRGBA(0xCD453DFF)) * 1.1f,
    colorToFloat(colorFromRGBA(0x298C0AFF)) * 1.1f,
    colorToFloat(colorFromRGBA(0x1D7EC5FF)) * 1.1f,
};

} // namespace gizmos
} // namespace gfx

#endif /* A1F9DAED_695B_4350_8992_7D7859A27481 */
