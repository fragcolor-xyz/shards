/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#pragma once

#include "./bgfx.hpp"
#include "blocks/shared.hpp"
#include "imgui.hpp"

namespace chainblocks {
namespace Gizmo {

struct Enums {
  enum class GridAxis { X, Y, Z };
  static constexpr uint32_t GridAxisCC = 'gizA';
  static inline EnumInfo<GridAxis> GridAxisEnumInfo{"GizmoGridAxis", CoreCC,
                                                    GridAxisCC};
  static inline Type GridAxisType = Type::Enum(CoreCC, GridAxisCC);
};

} // namespace Gizmo
} // namespace chainblocks
