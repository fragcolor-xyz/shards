/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#ifndef SH_EXTRA_GIZMO
#define SH_EXTRA_GIZMO

#include "./bgfx.hpp"
#include "shards/shared.hpp"
#include "imgui.hpp"

namespace shards {
namespace Gizmo {

struct Enums {
  enum class GridAxis { X, Y, Z };
  static constexpr uint32_t GridAxisCC = 'gizA';
  static inline EnumInfo<GridAxis> GridAxisEnumInfo{"GizmoGridAxis", CoreCC, GridAxisCC};
  static inline Type GridAxisType = Type::Enum(CoreCC, GridAxisCC);

  enum class Mode { Local, World };
  static constexpr uint32_t ModeCC = 'gizM';
  static inline EnumInfo<Mode> ModeEnumInfo{"GizmoMode", CoreCC, ModeCC};
  static inline Type ModeType = Type::Enum(CoreCC, ModeCC);

  enum class Operation { Translate, Rotate, Scale, Universal };
  static constexpr uint32_t OperationCC = 'gizO';
  static inline EnumInfo<Operation> OperationEnumInfo{"GizmoOperation", CoreCC, OperationCC};
  static inline Type OperationType = Type::Enum(CoreCC, OperationCC);

  static constexpr ImGuizmo::MODE ModeToGuizmo(SHEnum eval) {
    switch (Mode(eval)) {
    case Mode::Local:
      return ImGuizmo::MODE::LOCAL;
    case Mode::World:
      return ImGuizmo::MODE::WORLD;
    }
    return ImGuizmo::MODE(0);
  }

  static constexpr ImGuizmo::OPERATION OperationToGuizmo(SHEnum eval) {
    switch (Operation(eval)) {
    case Operation::Translate:
      return ImGuizmo::OPERATION::TRANSLATE;
    case Operation::Rotate:
      return ImGuizmo::OPERATION::ROTATE;
    case Operation::Scale:
      return ImGuizmo::OPERATION::SCALE;
    case Operation::Universal:
      return ImGuizmo::OPERATION::UNIVERSAL;
    }
    return ImGuizmo::OPERATION(0);
  }
};

} // namespace Gizmo
} // namespace shards

#endif // SH_EXTRA_GIZMO
