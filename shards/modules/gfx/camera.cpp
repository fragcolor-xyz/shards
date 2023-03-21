#include <shards/core/foundation.hpp>
#include <shards/core/params.hpp>
#include <shards/linalg_shim.hpp>
#include <gfx/math.hpp>
#include <gfx/window.hpp>
#include <gfx/fmt.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include <linalg/linalg.h>
#include "window.hpp"
#include "gfx.hpp"

using namespace shards;
using namespace shards::input;

namespace gfx {

// Mouse/Right stick rotation input and mouse X/Y movement (drag)
struct PointerInputState {
  float2 position{};
  float2 prevPosition{};
  bool primaryButton{};
  bool secondaryButton{};
  bool tertiaryButton{};
};

// Input state for a button axis
struct AxisInputState {
  bool pos{};
  bool neg{};

  float getValue() const {
    if (pos)
      return 1.0f;
    if (neg)
      return -1.0f;
    return 0.0f;
  }
};

// Tracked cursor & button states
struct InputState {
  AxisInputState keyboardXAxis;
  AxisInputState keyboardZAxis;
  PointerInputState pointer;
  float mouseWheel{};
  float deltaTime;
};

// Inputs for camera movement
struct CameraInputs {
  // XYZ camera translation (time scaled)
  float3 velocity;
  // XYZ camera translation (absolute)
  float3 translation;
  // Roll-Pitch-Yaw camera rotation
  float3 lookRotation;
};

// Update tracked buttons and pointers
inline void updateInputState(InputState &inputState, InputContext &inputContext) {
  inputState.pointer.prevPosition = inputState.pointer.position;
  inputState.pointer.position = inputContext.getState().cursorPosition;
  inputState.mouseWheel = inputContext.detached.getScrollDelta();

  for (auto &event : inputContext.getEvents()) {
    std::visit(
        [&](auto &&event) {
          using T = std::decay_t<decltype(event)>;
          if constexpr (std::is_same_v<T, KeyEvent>) {
            switch (event.key) {
            case SDL_KeyCode::SDLK_w:
              inputState.keyboardZAxis.neg = event.pressed;
              break;
            case SDL_KeyCode::SDLK_s:
              inputState.keyboardZAxis.pos = event.pressed;
              break;
            case SDL_KeyCode::SDLK_a:
              inputState.keyboardXAxis.neg = event.pressed;
              break;
            case SDL_KeyCode::SDLK_d:
              inputState.keyboardXAxis.pos = event.pressed;
              break;
            }
          } else if constexpr (std::is_same_v<T, PointerButtonEvent>) {
            inputState.pointer.position = float2(event.pos.x, event.pos.y);
            switch (event.index) {
            case SDL_BUTTON_LEFT:
              inputState.pointer.primaryButton = event.pressed;
              break;
            case SDL_BUTTON_RIGHT:
              inputState.pointer.secondaryButton = event.pressed;
              break;
            case SDL_BUTTON_MIDDLE:
              inputState.pointer.tertiaryButton = event.pressed;
              break;
            }
          } else if constexpr (std::is_same_v<T, ScrollEvent>) {
            inputState.mouseWheel += float(event.delta);
          }
        },
        event);
  }

  inputState.deltaTime = inputContext.deltaTime;
}

template <typename T> inline T getParamVarOrDefault(ParamVar &paramVar, const T &_default) {
  Var var(paramVar.get());
  return var.isNone() ? _default : (T)var;
}

struct FreeCameraShard {
  static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString help() { return SHCCSTR("Provides editor free camera controls"); }

  PARAM_PARAMVAR(_flySpeed, "FlySpeed", "Controls fly speed with the keyboard",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_scrollSpeed, "ScrollSpeed", "Controls middle mouse movement speed",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_panSpeed, "PanSpeed", "Controls middle mouse pan speed",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_lookSpeed, "LookSpeed", "Controls right mouse look speed",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_flySpeed), PARAM_IMPL_FOR(_scrollSpeed), PARAM_IMPL_FOR(_panSpeed), PARAM_IMPL_FOR(_lookSpeed));

  RequiredInputContext _inputContext;

  Mat4 _result;
  InputState _inputState;

  // Generates CameraInputs from an input state
  CameraInputs getCameraInputs(const InputState &inputState) {
    CameraInputs inputs;

    const float mouseBaseFactor = 2000.0f;

    float lookSpeed = getParamVarOrDefault(_lookSpeed, 1.0f) / mouseBaseFactor * 0.7f * gfx::pi2;
    float panSpeed = getParamVarOrDefault(_panSpeed, 1.0f) / mouseBaseFactor * 2.0f;
    float scrollSpeed = getParamVarOrDefault(_scrollSpeed, 1.0f);

    float2 pointerDelta = inputState.pointer.position - inputState.pointer.prevPosition;

    if (inputState.pointer.secondaryButton) {
      // Apply look rotation
      inputs.lookRotation.y = -pointerDelta.x * lookSpeed;
      inputs.lookRotation.x = -pointerDelta.y * lookSpeed;

      // Fly keys
      inputs.velocity.x += inputState.keyboardXAxis.getValue();
      inputs.velocity.z += inputState.keyboardZAxis.getValue();
    }

    if (inputState.pointer.tertiaryButton) {
      inputs.translation.x += -pointerDelta.x * panSpeed;
      inputs.translation.y += pointerDelta.y * panSpeed;
    }

    if (_inputState.mouseWheel != 0.0f) {
      inputs.translation.z += -_inputState.mouseWheel * scrollSpeed;
    }

    return inputs;
  }

  // Applies camera inputs to a view matrix
  float4x4 applyCameraInputsToView(const CameraInputs &inputs, const float4x4 &inputMatrix, float deltaTime) {
    float flySpeed = getParamVarOrDefault(_flySpeed, 1.0f) * 1.0f;

    // Output either a view matrix or camera matrix (inverse view matrix)
    const bool asViewMatrix = false;
    float4x4 invViewMatrix = asViewMatrix ? linalg::inverse(inputMatrix) : inputMatrix;

    float3 cameraTranslation;
    float3 cameraScale;
    float3x3 cameraRotationMatrix;
    gfx::decomposeTRS(invViewMatrix, cameraTranslation, cameraScale, cameraRotationMatrix);
    float4 cameraRotation = linalg::rotation_quat(cameraRotationMatrix);

    float3 cameraX = linalg::qxdir(cameraRotation);
    float3 cameraY = linalg::qydir(cameraRotation);
    float3 cameraZ = linalg::qzdir(cameraRotation);

    // Apply look rotation
    float4 rotX = linalg::rotation_quat(cameraX, inputs.lookRotation.x);
    float4 rotY = linalg::rotation_quat(cameraY, inputs.lookRotation.y);
    float4 rotZ = linalg::rotation_quat(cameraZ, inputs.lookRotation.z);
    cameraRotation = linalg::qmul(rotZ, cameraRotation);
    cameraRotation = linalg::qmul(rotX, cameraRotation);
    cameraRotation = linalg::qmul(rotY, cameraRotation);

    cameraTranslation +=
        (cameraX * inputs.velocity.x + cameraY * inputs.velocity.y + cameraZ * inputs.velocity.z) * flySpeed * deltaTime;
    cameraTranslation += (cameraX * inputs.translation.x + cameraY * inputs.translation.y + cameraZ * inputs.translation.z);

    // Reconstruct view matrix using the inverse rotation/translation of the camera
    float4x4 outputMatrix = linalg::rotation_matrix(asViewMatrix ? linalg::qconj(cameraRotation) : cameraRotation);
    outputMatrix = linalg::mul(linalg::translation_matrix(asViewMatrix ? -cameraTranslation : cameraTranslation), outputMatrix);

    return outputMatrix;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    updateInputState(_inputState, _inputContext);
    bool anyButtonHeld = _inputState.pointer.secondaryButton || _inputState.pointer.tertiaryButton;
    if (anyButtonHeld) {
      _inputContext->requestFocus = true;
      _inputContext->wantsPointerInput = true;
      _inputContext->wantsKeyboardInput = true;
    } else {
      _inputContext->requestFocus = false;
      _inputContext->wantsPointerInput = true;
      _inputContext->wantsKeyboardInput = false;
    }

    CameraInputs cameraInputs = getCameraInputs(_inputState);

    float4x4 viewMatrix = (Mat4)input;
    _result = applyCameraInputsToView(cameraInputs, viewMatrix, _inputState.deltaTime);

    return _result;
  }

  void cleanup() {
    PARAM_CLEANUP();
    _inputContext.cleanup();
  }
  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _inputContext.warmup(context);
  }

  SHTypeInfo compose(SHInstanceData &data) { return outputTypes().elements[0]; }

  SHExposedTypesInfo requiredVariables() {
    static auto e = exposedTypesOf(RequiredInputContext::getExposedTypeInfo());
    return e;
  }
};

// struct PivotCameraShard {
//   static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }
//   static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
//   static SHOptionalString help() { return SHCCSTR("Provides editor free camera controls"); }

//   PARAM_PARAMVAR(_flySpeed, "FlySpeed", "Controls fly speed with the keyboard",
//                  {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
//   PARAM_PARAMVAR(_scrollSpeed, "ScrollSpeed", "Controls middle mouse movement speed",
//                  {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
//   PARAM_PARAMVAR(_panSpeed, "PanSpeed", "Controls middle mouse pan speed",
//                  {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
//   PARAM_PARAMVAR(_lookSpeed, "LookSpeed", "Controls right mouse look speed",
//                  {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
//   PARAM_IMPL(PARAM_IMPL_FOR(_flySpeed), PARAM_IMPL_FOR(_scrollSpeed), PARAM_IMPL_FOR(_panSpeed), PARAM_IMPL_FOR(_lookSpeed));

//   RequiredGraphicsContext _graphicsContext;
//   RequiredWindowContext _windowContext;

//   Mat4 _result;
//   InputState _inputState;

//   float3 _pivot{};
//   float _distance{};

//   // Generates CameraInputs from an input state
//   CameraInputs getCameraInputs(const InputState &inputState) {
//     CameraInputs inputs;

//     const float mouseBaseFactor = 2000.0f;

//     float lookSpeed = getParamVarOrDefault(_lookSpeed, 1.0f) / mouseBaseFactor * 0.7f * gfx::pi2;
//     float panSpeed = getParamVarOrDefault(_panSpeed, 1.0f) / mouseBaseFactor * 2.0f;
//     float scrollSpeed = getParamVarOrDefault(_scrollSpeed, 1.0f);

//     float2 pointerDelta = inputState.pointer.position - inputState.pointer.prevPosition;

//     if (inputState.pointer.secondaryButton) {
//       // Apply look rotation
//       inputs.lookRotation.y = -pointerDelta.x * lookSpeed;
//       inputs.lookRotation.x = -pointerDelta.y * lookSpeed;

//       // Fly keys
//       inputs.velocity.x += inputState.keyboardXAxis.getValue();
//       inputs.velocity.z += inputState.keyboardZAxis.getValue();
//     }

//     if (inputState.pointer.tertiaryButton) {
//       inputs.translation.x += -pointerDelta.x * panSpeed;
//       inputs.translation.y += pointerDelta.y * panSpeed;
//     }

//     if (_inputState.mouseWheel != 0.0f) {
//       inputs.translation.z += -_inputState.mouseWheel * scrollSpeed;
//     }

//     return inputs;
//   }

//   // Applies camera inputs to a view matrix
//   float4x4 applyCameraInputsToView(const CameraInputs &inputs, const float4x4 &viewMatrix, float deltaTime) {
//     float flySpeed = getParamVarOrDefault(_flySpeed, 1.0f) * 1.0f;

//     float4x4 invViewMatrix = linalg::inverse(viewMatrix);

//     float3 cameraTranslation;
//     float3 cameraScale;
//     float3x3 cameraRotationMatrix;
//     gfx::decomposeTRS(invViewMatrix, cameraTranslation, cameraScale, cameraRotationMatrix);
//     float4 cameraRotation = linalg::rotation_quat(cameraRotationMatrix);

//     float3 cameraX = linalg::qxdir(cameraRotation);
//     float3 cameraY = linalg::qydir(cameraRotation);
//     float3 cameraZ = linalg::qzdir(cameraRotation);

//     // Apply look rotation
//     float4 rotX = linalg::rotation_quat(cameraX, inputs.lookRotation.x);
//     float4 rotY = linalg::rotation_quat(cameraY, inputs.lookRotation.y);
//     float4 rotZ = linalg::rotation_quat(cameraZ, inputs.lookRotation.z);
//     cameraRotation = linalg::qmul(rotZ, cameraRotation);
//     cameraRotation = linalg::qmul(rotX, cameraRotation);
//     cameraRotation = linalg::qmul(rotY, cameraRotation);

//     cameraTranslation +=
//         (cameraX * inputs.velocity.x + cameraY * inputs.velocity.y + cameraZ * inputs.velocity.z) * flySpeed * deltaTime;
//     cameraTranslation += (cameraX * inputs.translation.x + cameraY * inputs.translation.y + cameraZ * inputs.translation.z);

//     // Reconstruct view matrix using the inverse rotation/translation of the camera
//     float4x4 newViewMatrix = linalg::identity;
//     newViewMatrix = linalg::mul(linalg::translation_matrix(-cameraTranslation), newViewMatrix);
//     newViewMatrix = linalg::mul(linalg::rotation_matrix(linalg::qconj(cameraRotation)), newViewMatrix);

//     return newViewMatrix;
//   }

//   void setInitialPivot(const float4x4 &initViewMatrix) {
//     float4x4 invViewMatrix = linalg::inverse(initViewMatrix);

//     float3 cameraTranslation;
//     float3 cameraScale;
//     float3x3 cameraRotationMatrix;
//     gfx::decomposeTRS(invViewMatrix, cameraTranslation, cameraScale, cameraRotationMatrix);
//     float4 cameraRotation = linalg::rotation_quat(cameraRotationMatrix);

//     float3 zDir = linalg::qzdir(cameraRotation);
//     _distance = 10.0f; // Default distance
//     _pivot = cameraTranslation + zDir * _distance;
//   }

//   SHVar activate(SHContext *context, const SHVar &input) {
//     float4x4 viewMatrix = (Mat4)input;

//     if (_distance <= 0.0f)
//       setInitialPivot(viewMatrix);

//     updateInputState(_inputState, _windowContext, _graphicsContext);

//     CameraInputs cameraInputs = getCameraInputs(_inputState);

//     _result = applyCameraInputsToView(cameraInputs, viewMatrix, _inputState.deltaTime);

//     return _result;
//   }

//   void cleanup() {
//     PARAM_CLEANUP();
//     _graphicsContext.cleanup();
//     _windowContext.cleanup();
//   }
//   void warmup(SHContext *context) {
//     PARAM_WARMUP(context);
//     _graphicsContext.warmup(context);
//     _windowContext.warmup(context);
//   }

//   SHTypeInfo compose(SHInstanceData &data) {
//     composeCheckGfxThread(data);
//     return outputTypes().elements[0];
//   }

//   SHExposedTypesInfo requiredVariables() {
//     static auto e = exposedTypesOf(RequiredGraphicsContext::getExposedTypeInfo(), RequiredWindowContext::getExposedTypeInfo());
//     return e;
//   }
// };

void registerCameraShards() {
  REGISTER_SHARD("FreeCamera", FreeCameraShard);
  // REGISTER_SHARD("PivotCamera", PivotCameraShard);
}

} // namespace gfx
