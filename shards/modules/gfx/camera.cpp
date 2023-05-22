#include <shards/core/foundation.hpp>
#include <shards/core/params.hpp>
#include <shards/linalg_shim.hpp>
#include <gfx/math.hpp>
#include <gfx/window.hpp>
#include <gfx/fmt.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include "gfx.hpp"

using namespace shards;
using namespace shards::Inputs;

namespace gfx {

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
  PARAM_IMPL(FreeCameraShard, PARAM_IMPL_FOR(_flySpeed), PARAM_IMPL_FOR(_scrollSpeed), PARAM_IMPL_FOR(_panSpeed),
             PARAM_IMPL_FOR(_lookSpeed));

  RequiredGraphicsContext _graphicsContext;
  RequiredInputContext _inputContext;

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
  } _inputState;

  // Inputs for camera movement
  struct CameraInputs {
    // XYZ camera translation (time scaled)
    float3 velocity;
    // XYZ camera translation (absolute)
    float3 translation;
    // Roll-Pitch-Yaw camera rotation
    float3 lookRotation;
  };

  Mat4 _result;

  // Update tracked buttons and pointers
  void updateInputState() {
    _inputState.pointer.prevPosition = _inputState.pointer.position;
    _inputState.mouseWheel = 0.0f;

    for (auto &event : _inputContext->events) {
      switch (event.type) {
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        switch (event.key.keysym.sym) {
        case SDL_KeyCode::SDLK_w:
          _inputState.keyboardZAxis.neg = event.key.type == SDL_KEYDOWN;
          break;
        case SDL_KeyCode::SDLK_s:
          _inputState.keyboardZAxis.pos = event.key.type == SDL_KEYDOWN;
          break;
        case SDL_KeyCode::SDLK_a:
          _inputState.keyboardXAxis.neg = event.key.type == SDL_KEYDOWN;
          break;
        case SDL_KeyCode::SDLK_d:
          _inputState.keyboardXAxis.pos = event.key.type == SDL_KEYDOWN;
          break;
        }
        break;
      case SDL_MOUSEMOTION:
        _inputState.pointer.position = float2(event.motion.x, event.motion.y);
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        _inputState.pointer.position = float2(event.button.x, event.button.y);
        switch (event.button.button) {
        case SDL_BUTTON_LEFT:
          _inputState.pointer.primaryButton = event.button.type == SDL_MOUSEBUTTONDOWN;
          break;
        case SDL_BUTTON_RIGHT:
          _inputState.pointer.secondaryButton = event.button.type == SDL_MOUSEBUTTONDOWN;
          break;
        case SDL_BUTTON_MIDDLE:
          _inputState.pointer.tertiaryButton = event.button.type == SDL_MOUSEBUTTONDOWN;
          break;
        }
        break;
      case SDL_MOUSEWHEEL:
        _inputState.mouseWheel += float(event.wheel.preciseY);
        break;
      }
    }

    _inputState.deltaTime = _graphicsContext->deltaTime;
  }

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
  float4x4 applyCameraInputsToView(const CameraInputs &inputs, const float4x4 &viewMatrix, float deltaTime) {
    float flySpeed = getParamVarOrDefault(_flySpeed, 1.0f) * 1.0f;

    float4x4 invViewMatrix = linalg::inverse(viewMatrix);

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
    float4x4 newViewMatrix = linalg::identity;
    newViewMatrix = linalg::mul(linalg::translation_matrix(-cameraTranslation), newViewMatrix);
    newViewMatrix = linalg::mul(linalg::rotation_matrix(linalg::qconj(cameraRotation)), newViewMatrix);

    return newViewMatrix;
  }

  template <typename T> T getParamVarOrDefault(ParamVar &paramVar, const T &_default) {
    Var var(paramVar.get());
    return var.isNone() ? _default : (T)var;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    updateInputState();

    CameraInputs cameraInputs = getCameraInputs(_inputState);

    float4x4 viewMatrix = (Mat4)input;
    _result = applyCameraInputsToView(cameraInputs, viewMatrix, _inputState.deltaTime);

    return _result;
  }

  void cleanup() {
    PARAM_CLEANUP();
    _graphicsContext.cleanup();
    _inputContext.cleanup();
  }
  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _graphicsContext.warmup(context);
    _inputContext.warmup(context);
  }

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckGfxThread(data);
    return outputTypes().elements[0];
  }

  SHExposedTypesInfo requiredVariables() {
    static auto e = exposedTypesOf(RequiredGraphicsContext::getExposedTypeInfo(), RequiredInputContext::getExposedTypeInfo());
    return e;
  }
};

void registerCameraShards() { REGISTER_SHARD("FreeCamera", FreeCameraShard); }

} // namespace gfx
