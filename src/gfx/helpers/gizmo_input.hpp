#ifndef FA3CD16D_2B62_442D_AEC9_50A3AE9EFD8C
#define FA3CD16D_2B62_442D_AEC9_50A3AE9EFD8C

#include "../fwd.hpp"
#include "../linalg.hpp"
#include "../math.hpp"
#include "../view.hpp"
#include <memory>
#include <optional>

namespace gfx {
namespace gizmos {

struct Box {
  float3 min{};
  float3 max{};
};

struct InputContext;
struct Handle;

// Callback handler for gizmo handles
struct IGizmoCallbacks {
  virtual ~IGizmoCallbacks() = default;

  // Called when handle grabbed
  virtual void grabbed(InputContext &context, Handle &handle) = 0;
  // Called when handle released
  virtual void released(InputContext &context, Handle &handle) = 0;
  // Called every update while handle is being held
  virtual void move(InputContext &context, Handle &handle) = 0;
};

struct Handle {
  Box selectionBox;
  float4x4 selectionBoxTransform;
  IGizmoCallbacks *callbacks{};
  void *userData{};
};

struct InputState {
  int2 cursorPosition{};
  int2 viewSize{};
  bool pressed{};
};

// Input tracking for gizmo handles
// use withing a loop where you call
// - begin
// - updateHandle (for each handle in the view)
// - end (this will run callbacks)
// user is responsible for keeping handle data alive between begin/end
struct InputContext {
public:
  // Currently held handle
  // it will keep being held until the interation button is released
  Handle *held{};
  bool heldHandleUpdated = false;

  float3 eyeLocation;
  float3 rayDirection;

  // Currently hovered handle
  // will be locked to the held handle while the interaction button is pressed
  Handle *hovered{};

  // Information about handle that was hit by cursor ray
  float3 hitLocation;
  float hitDistance;

  // Last input state
  InputState inputState;
  InputState prevInputState;

private:
  float4x4 cachedViewProj;
  float4x4 cachedViewProjInv;

public:
  void begin(const InputState &inputState, ViewPtr view);
  // Call this within the update for each handle in view
  //  when a handle is grabbed, this is allowed to run the move callback directly
  //  since no raycast needs to be performed on the entire set of handles
  void updateHandle(Handle &handle);
  // Call to end input update and run input callbacks
  void end();

private:
  // Computes eye location and cursor ray direction
  void updateView(ViewPtr view);
  // Updates movement for the currently held handle
  void updateHeldHandle();
  // Updates `hitLocation` variable based on hovered/held handle
  void updateHitLocation();
};
} // namespace gizmos

} // namespace gfx

#endif /* FA3CD16D_2B62_442D_AEC9_50A3AE9EFD8C */
