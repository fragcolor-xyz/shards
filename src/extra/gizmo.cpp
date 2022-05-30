/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "gizmo.hpp"

using namespace shards;

namespace shards {
namespace Gizmo {
static TableVar experimental{{"experimental", Var(true)}};

namespace Helper {
static const float identity[16] = {
    1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
};

const float PI = 3.141592653589793238463f;
const float PI_2 = PI * 0.5f;
}; // namespace Helper

struct CubeView : public BGFX::BaseConsumer {
  static SHOptionalString help() {
    return SHCCSTR("Display a cube gizmo that can be used to control the "
                   "orientation of the current camera.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is not used and will pass through."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _view = value;
      break;

    default:
      throw SHException("Parameter out of range.");
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _view;

    default:
      throw SHException("Parameter out of range.");
    }
  }

  void cleanup() { _view.cleanup(); }

  void warmup(SHContext *context) { _view.warmup(context); }

  SHExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = BGFX::BaseConsumer::ContextInfo;
    idx++;

    if (_view.isVariable()) {
      _required[idx].name = _view.variableName();
      _required[idx].help = SHCCSTR("The required view matrix.");
      _required[idx].exposedType = CoreInfo::Float4x4Type;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &view = _view.get();
    // HACKish, I use Mat4 because the API hides some complexity, but it feels
    // unnecessary to do that many copies should manipulate the seq of float4
    // directly
    Mat4 mat{};
    mat = view;
    aligned_array<float, 16> arrView;
    Mat4::ToArrayUnsafe(mat, arrView.data());

    ImGuiIO &io = ::ImGui::GetIO();
    float viewManipulateRight = io.DisplaySize.x; // TODO: make it a param (% of viewport?)
    float viewManipulateTop = 0;                  // TODO: make it a param
    ImGuizmo::ViewManipulate(arrView.data(), 5, ImVec2(viewManipulateRight - 128, viewManipulateTop), ImVec2(128, 128),
                             0x10101010); // TODO: make offset and size a param

    shards::cloneVar(view, Mat4::FromArrayUnsafe(arrView.data()));

    return input;
  }

private:
  static inline Parameters _params = {{"ViewMatrix",
                                       SHCCSTR("A variable that points to the vuew matrix that will be modified."),
                                       {Type::VariableOf(CoreInfo::Float4x4Type)}}};

  ParamVar _view{};
  std::array<SHExposedTypeInfo, 2> _required;
};

struct Grid : public BGFX::BaseConsumer {
  static SHOptionalString help() { return SHCCSTR("Displays a grid."); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is not used and will pass through."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _axis = value;
      break;
    case 1:
      _size = value;
      break;

    default:
      throw SHException("Parameter out of range.");
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _axis;
    case 1:
      return _size;

    default:
      throw SHException("Parameter out of range.");
    }
  }

  void cleanup() {
    _axis.cleanup();
    _size.cleanup();

    BGFX::BaseConsumer::_cleanup();
  }

  void warmup(SHContext *context) {
    BGFX::BaseConsumer::_warmup(context);

    _axis.warmup(context);
    _size.warmup(context);
  }

  SHExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = BGFX::BaseConsumer::ContextInfo;
    idx++;

    if (_axis.isVariable()) {
      _required[idx].name = _axis.variableName();
      _required[idx].help = SHCCSTR("The required axis.");
      _required[idx].exposedType = Enums::GridAxisType;
      idx++;
    }
    if (_size.isVariable()) {
      _required[idx].name = _size.variableName();
      _required[idx].help = SHCCSTR("The required size.");
      _required[idx].exposedType = CoreInfo::FloatType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) {

    linalg::aliases::float4 rot;
    switch (Enums::GridAxis(_axis.get().payload.enumValue)) {
    case Enums::GridAxis::X:
      rot = linalg::rotation_quat(AxisZ, Helper::PI_2);
      break;
    case Enums::GridAxis::Y:
      rot = {0.f, 0.f, 0.f, 1.f};
      break;
    case Enums::GridAxis::Z:
      rot = linalg::rotation_quat(AxisX, Helper::PI_2);
      break;
    }

    auto *ctx = reinterpret_cast<BGFX::Context *>(_bgfxCtx->payload.objectValue);
    const auto &currentView = ctx->currentView();

    _matView = linalg::mul(currentView.view, linalg::rotation_matrix(rot));

    aligned_array<float, 16> arrView;
    aligned_array<float, 16> arrProj;
    Mat4::ToArrayUnsafe(_matView, arrView.data());
    Mat4::ToArrayUnsafe(currentView.proj, arrProj.data());

    ImGuiIO &io = ::ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::DrawGrid(arrView.data(), arrProj.data(), Helper::identity, _size.get().payload.floatValue);

    return input;
  }

private:
  static inline Parameters _params = {
      // TODO: allow to pass a user (float3) as axis?)
      {"Axis", SHCCSTR("The normal axis of the grid plane"), {Enums::GridAxisType, Type::VariableOf(Enums::GridAxisType)}},
      {"Size", SHCCSTR("The size of the extents of the grid in all directions."), {CoreInfo::FloatType, CoreInfo::FloatVarType}},
  };

  // params
  ParamVar _axis{Var((SHEnum)Enums::GridAxis::Y)};
  ParamVar _size{Var(100.0f)};
  std::array<SHExposedTypeInfo, 3> _required;

  Mat4 _matView{};
};

struct Transform : public BGFX::BaseConsumer {
  static SHOptionalString help() {
    return SHCCSTR("Displays a gizmo that can be used to move, rotate or scale "
                   "an object.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is not used and will pass through."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _matrix = value;
      break;
    case 1:
      _mode = value;
      break;
    case 2:
      _operation = value;
      break;
    case 3:
      _snap = value;
      break;

    default:
      throw SHException("Parameter out of range.");
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _matrix;
    case 1:
      return _mode;
    case 2:
      return _operation;
    case 3:
      return _snap;

    default:
      throw SHException("Parameter out of range.");
    }
  }

  void cleanup() {
    _matrix.cleanup();
    _mode.cleanup();
    _operation.cleanup();
    _snap.cleanup();

    BGFX::BaseConsumer::_cleanup();
  }

  void warmup(SHContext *context) {
    BGFX::BaseConsumer::_warmup(context);

    _matrix.warmup(context);
    _mode.warmup(context);
    _operation.warmup(context);
    _snap.warmup(context);
  }

  SHExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = BGFX::BaseConsumer::ContextInfo;
    idx++;

    if (_matrix.isVariable()) {
      _required[idx].name = _matrix.variableName();
      _required[idx].help = SHCCSTR("The required matrix.");
      _required[idx].exposedType = CoreInfo::Float4x4Type;
      idx++;
    }
    if (_mode.isVariable()) {
      _required[idx].name = _mode.variableName();
      _required[idx].help = SHCCSTR("The required mode.");
      _required[idx].exposedType = Enums::ModeType;
      idx++;
    }
    if (_operation.isVariable()) {
      _required[idx].name = _operation.variableName();
      _required[idx].help = SHCCSTR("The required operation.");
      _required[idx].exposedType = Enums::OperationType;
      idx++;
    }
    if (_snap.isVariable()) {
      _required[idx].name = _snap.variableName();
      _required[idx].help = SHCCSTR("The optional snap.");
      _required[idx].exposedType = CoreInfo::Float3Type;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto *ctx = reinterpret_cast<BGFX::Context *>(_bgfxCtx->payload.objectValue);
    const auto &currentView = ctx->currentView();

    aligned_array<float, 16> arrView;
    aligned_array<float, 16> arrProj;
    Mat4::ToArrayUnsafe(currentView.view, arrView.data());
    Mat4::ToArrayUnsafe(currentView.proj, arrProj.data());

    auto &matrix = _matrix.get();
    // HACKish, I use Mat4 because the API hides some complexity, but it feels
    // unnecessary to do that many copies should manipulate the seq of float4
    // directly
    Mat4 mat{};
    mat = matrix;
    aligned_array<float, 16> arrMatrix;
    Mat4::ToArrayUnsafe(mat, arrMatrix.data());

    auto &snap = _snap.get();
    auto useSnap = snap.valueType == SHType::Float3;
    auto snapPtr = useSnap ? reinterpret_cast<float *>(&snap.payload.float3Value) : nullptr;

    ImGuizmo::Manipulate(arrView.data(), arrProj.data(), Enums::OperationToGuizmo(_operation.get().payload.enumValue),
                         Enums::ModeToGuizmo(_mode.get().payload.enumValue), arrMatrix.data(), nullptr, snapPtr);

    shards::cloneVar(matrix, Mat4::FromArrayUnsafe(arrMatrix.data()));

    return input;
  }

private:
  static inline Parameters _params = {
      {"Matrix", SHCCSTR("Object matrix modified by this transform gizmo."), {Type::VariableOf(CoreInfo::Float4x4Type)}},
      {"Mode",
       SHCCSTR("Type of coordinates used. Either local to the object or global "
               "(world)."),
       {Enums::ModeType, Type::VariableOf(Enums::ModeType)}},
      {"Operation",
       SHCCSTR("Operation to apply (translation, rotation or scaling)."),
       {Enums::OperationType, Type::VariableOf(Enums::OperationType)}},
      {"Snap",
       SHCCSTR("Snapping values on all 3 axes. World unit for translation. "
               "Angle in degrees for rotation. Multiplier for scaling. Use "
               "`None` or `(float3 0 0 0)` to disable."),
       {CoreInfo::NoneType, CoreInfo::Float3Type, CoreInfo::Float3VarType}}};

  ParamVar _matrix{};
  ParamVar _mode{};
  ParamVar _operation{};
  ParamVar _snap{};
  std::array<SHExposedTypeInfo, 5> _required;
};

void registerGizmoShards() {
  REGISTER_SHARD("Gizmo.CubeView", CubeView);
  REGISTER_SHARD("Gizmo.Grid", Grid);
  REGISTER_SHARD("Gizmo.Transform", Transform);
}
}; // namespace Gizmo
}; // namespace shards
