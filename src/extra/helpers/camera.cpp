#ifndef D0D31C79_6DAC_4176_B686_B1E579CCEC65
#define D0D31C79_6DAC_4176_B686_B1E579CCEC65

#include <foundation.hpp>
#include <params.hpp>
#include <linalg_shim.hpp>
#include <gfx/math.hpp>
#include <gfx/window.hpp>
#include <gfx/fmt.hpp>
#include "../gfx.hpp"

namespace shards {
namespace Helpers {
using namespace linalg::aliases;

struct FreeCameraShard : public gfx::BaseConsumer {
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

  struct InputState {
    struct Keys {
      bool left{};
      bool right{};
      bool forward{};
      bool backward{};
    } keys;
    struct Look {
      float2 cursorPosition{};
      float2 prevCursorPosition{};
      bool primary{};
      bool secondary{};
      bool tertiary{};
    } look;
    float zoom{};
  } _input;

  Mat4 _result;

  void updateInput() {
    auto &globals = getMainWindowGlobals();

    _input.look.prevCursorPosition = _input.look.cursorPosition;
    _input.zoom = 0.0f;

    for (auto &event : globals.events) {
      switch (event.type) {
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        switch (event.key.keysym.sym) {
        case SDL_KeyCode::SDLK_w:
          _input.keys.forward = event.key.type == SDL_KEYDOWN;
          break;
        case SDL_KeyCode::SDLK_a:
          _input.keys.left = event.key.type == SDL_KEYDOWN;
          break;
        case SDL_KeyCode::SDLK_s:
          _input.keys.backward = event.key.type == SDL_KEYDOWN;
          break;
        case SDL_KeyCode::SDLK_d:
          _input.keys.right = event.key.type == SDL_KEYDOWN;
          break;
        }
        break;
      case SDL_MOUSEMOTION:
        _input.look.cursorPosition = getWindow().toVirtualCoord(float2(event.motion.x, event.motion.y));
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        _input.look.cursorPosition = getWindow().toVirtualCoord(float2(event.button.x, event.button.y));
        switch (event.button.button) {
        case SDL_BUTTON_LEFT:
          _input.look.primary = event.button.type == SDL_MOUSEBUTTONDOWN;
          break;
        case SDL_BUTTON_RIGHT:
          _input.look.secondary = event.button.type == SDL_MOUSEBUTTONDOWN;
          break;
        case SDL_BUTTON_MIDDLE:
          _input.look.tertiary = event.button.type == SDL_MOUSEBUTTONDOWN;
          break;
        }
        break;
      case SDL_MOUSEWHEEL:
        _input.zoom += float(event.wheel.preciseY);
        break;
      }
    }
  }

  template <typename T> T getParamVarOrDefault(ParamVar &paramVar, const T &_default) {
    Var var(paramVar.get());
    return var.isNone() ? _default : (T)var;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    updateInput();

    const float mouseBaseFactor = 2000.0f;

    float flySpeed = getParamVarOrDefault(_flySpeed, 1.0f) * 1.0f;
    float lookSpeed = getParamVarOrDefault(_lookSpeed, 1.0f) / mouseBaseFactor * 0.7f * gfx::pi2;
    float scrollSpeed = getParamVarOrDefault(_scrollSpeed, 1.0f);
    float panSpeed = getParamVarOrDefault(_panSpeed, 1.0f) / mouseBaseFactor * 2.0f;

    float4x4 viewMatrix = (Mat4)input;
    float4x4 invViewMatrix = linalg::inverse(viewMatrix);

    float3 cameraTranslation;
    float3 cameraScale;
    float3x3 cameraRotationMatrix;
    gfx::decomposeTRS(invViewMatrix, cameraTranslation, cameraScale, cameraRotationMatrix);
    float4 cameraRotation = linalg::rotation_quat(cameraRotationMatrix);

    float3 cameraX = linalg::qxdir(cameraRotation);
    float3 cameraY = linalg::qydir(cameraRotation);
    float3 cameraZ = linalg::qzdir(cameraRotation);

    float2 cursorDelta = _input.look.cursorPosition - _input.look.prevCursorPosition;

    if (_input.look.secondary) {
      // Apply look rotation
      float4 rotY = linalg::rotation_quat(cameraY, -cursorDelta.x * lookSpeed);
      float4 rotX = linalg::rotation_quat(cameraX, -cursorDelta.y * lookSpeed);
      cameraRotation = linalg::qmul(rotX, cameraRotation);
      cameraRotation = linalg::qmul(rotY, cameraRotation);

      // Fly keys
      float2 wasdVec{};
      if (_input.keys.left)
        wasdVec.x += -1.0f;
      if (_input.keys.right)
        wasdVec.x += 1.0f;
      if (_input.keys.forward)
        wasdVec.y += 1.0f;
      if (_input.keys.backward)
        wasdVec.y += -1.0f;

      // TODO
      const float dt = 1.0f / 120.0f;
      cameraTranslation += (-cameraZ * wasdVec.y + cameraX * wasdVec.x) * flySpeed * dt;
    }

    if (_input.look.tertiary) {
      cameraTranslation += -cameraX * cursorDelta.x * panSpeed;
      cameraTranslation += cameraY * cursorDelta.y * panSpeed;
    }

    if (_input.zoom != 0.0f) {
      cameraTranslation += cameraZ * -_input.zoom * scrollSpeed;
    }

    // Reconstruct view matrix using the inverse rotation/translation of the camera
    float4x4 newViewMatrix = linalg::identity;
    newViewMatrix = linalg::mul(linalg::translation_matrix(-cameraTranslation), newViewMatrix);
    newViewMatrix = linalg::mul(linalg::rotation_matrix(linalg::qconj(cameraRotation)), newViewMatrix);

    _result = newViewMatrix;
    return _result;
  }

  void cleanup() {
    PARAM_CLEANUP();
    baseConsumerCleanup();
  }
  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseConsumerWarmup(context);
  }

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckMainThread(data);
    composeCheckMainWindowGlobals(data);
    return outputTypes().elements[0];
  }
};

void registerCameraShards() { REGISTER_SHARD("Helpers.FreeCamera", FreeCameraShard); }

} // namespace Helpers
} // namespace shards

#endif /* D0D31C79_6DAC_4176_B686_B1E579CCEC65 */
