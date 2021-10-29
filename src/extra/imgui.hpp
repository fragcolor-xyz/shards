/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#pragma once

#include "foundation.hpp"
#include "imgui.h"

#include "ImGuizmo.h"

namespace ImGuiExtra {
#include "imgui_memory_editor.h"
};

namespace chainblocks {
namespace ImGui {
constexpr uint32_t ImGuiContextCC = 'gui ';

struct Context {
  static inline Type Info{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = ImGuiContextCC}}}};

  // Useful to compare with with plugins, they might mismatch!
  static inline const char *Version = ::ImGui::GetVersion();

  // ImGuiContext *context = ::ImGui::CreateContext();

  // ~Context() { ::ImGui::DestroyContext(context); }

  // void Set() { ::ImGui::SetCurrentContext(context); }

  // void Reset() {
  //   ::ImGui::DestroyContext(context);
  //   context = ::ImGui::CreateContext();
  // }
};

struct Enums {
  enum class GuiDir {
    Left = ::ImGuiDir_Left,
    Right = ::ImGuiDir_Right,
    Up = ::ImGuiDir_Up,
    Down = ::ImGuiDir_Down
  };
  static constexpr uint32_t GuiDirCC = 'guiD';
  static inline EnumInfo<GuiDir> GuiDirEnumInfo{"GuiDir", CoreCC, GuiDirCC};
  static inline Type GuiDirType = Type::Enum(CoreCC, GuiDirCC);

  enum class GuiTableFlags {
    None = ::ImGuiTableFlags_None,
    Resizable = ::ImGuiTableFlags_Resizable,
    Reorderable = ::ImGuiTableFlags_Reorderable,
    Hideable = ::ImGuiTableFlags_Hideable,
    Sortable = ::ImGuiTableFlags_Sortable
  };
  static constexpr uint32_t GuiTableFlagsCC = 'guTF';
  static inline EnumInfo<GuiTableFlags> GuiTableFlagsEnumInfo{
      "GuiTableFlags", CoreCC, GuiTableFlagsCC};
  static inline Type GuiTableFlagsType = Type::Enum(CoreCC, GuiTableFlagsCC);

  enum class GuiTableColumnFlags {
    None = ::ImGuiTableColumnFlags_None,
    Disabled = ::ImGuiTableColumnFlags_Disabled,
    DefaultHide = ::ImGuiTableColumnFlags_DefaultHide,
    DefaultSort = ::ImGuiTableColumnFlags_DefaultSort
  };
  static constexpr uint32_t GuiTableColumnFlagsCC = 'gTCF';
  static inline EnumInfo<GuiTableFlags> GuiTableColumnFlagsEnumInfo{
      "GuiTableColumnFlags", CoreCC, GuiTableColumnFlagsCC};
  static inline Type GuiTableColumnFlagsType =
      Type::Enum(CoreCC, GuiTableColumnFlagsCC);
};
}; // namespace ImGui
}; // namespace chainblocks
