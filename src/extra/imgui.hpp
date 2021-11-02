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
  enum class GuiButtonKind {
    Normal,
    Small,
    Invisible,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown
  };
  static constexpr uint32_t GuiButtonKindCC = 'guiD';
  static inline EnumInfo<GuiButtonKind> GuiButtonKindEnumInfo{
      "GuiButton", CoreCC, GuiButtonKindCC};
  static inline Type GuiButtonKindType = Type::Enum(CoreCC, GuiButtonKindCC);

  enum class GuiTableFlags {
    None = ::ImGuiTableFlags_None,
    Resizable = ::ImGuiTableFlags_Resizable,
    Reorderable = ::ImGuiTableFlags_Reorderable,
    Hideable = ::ImGuiTableFlags_Hideable,
    Sortable = ::ImGuiTableFlags_Sortable
  };
  static constexpr uint32_t GuiTableFlagsCC = 'guTF';
  static inline FlagsInfo<GuiTableFlags> GuiTableFlagsEnumInfo{
      "GuiTableFlags", CoreCC, GuiTableFlagsCC};
  static inline Type GuiTableFlagsType = Type::Enum(CoreCC, GuiTableFlagsCC);

  enum class GuiTableColumnFlags {
    None = ::ImGuiTableColumnFlags_None,
    Disabled = ::ImGuiTableColumnFlags_Disabled,
    DefaultHide = ::ImGuiTableColumnFlags_DefaultHide,
    DefaultSort = ::ImGuiTableColumnFlags_DefaultSort
  };
  static constexpr uint32_t GuiTableColumnFlagsCC = 'gTCF';
  static inline FlagsInfo<GuiTableColumnFlags> GuiTableColumnFlagsEnumInfo{
      "GuiTableColumnFlags", CoreCC, GuiTableColumnFlagsCC};
  static inline Type GuiTableColumnFlagsType =
      Type::Enum(CoreCC, GuiTableColumnFlagsCC);

  enum class GuiTreeNodeFlags {
    None = ::ImGuiTreeNodeFlags_None,
    Selected = ::ImGuiTreeNodeFlags_Selected,
    Framed = ::ImGuiTreeNodeFlags_Framed,
    OpenOnDoubleClick = ::ImGuiTreeNodeFlags_OpenOnDoubleClick,
    OpenOnArrow = ::ImGuiTreeNodeFlags_OpenOnArrow,
    Leaf = ::ImGuiTreeNodeFlags_Leaf,
    Bullet = ::ImGuiTreeNodeFlags_Bullet,
    SpanAvailWidth = ::ImGuiTreeNodeFlags_SpanAvailWidth,
    SpanFullWidth = ::ImGuiTreeNodeFlags_SpanFullWidth
  };
  static constexpr uint32_t GuiTreeNodeFlagsCC = 'gTNF';
  static inline FlagsInfo<GuiTreeNodeFlags> GuiTreeNodeFlagsEnumInfo{
      "GuiTreeNodeFlags", CoreCC, GuiTreeNodeFlagsCC};
  static inline Type GuiTreeNodeFlagsType =
      Type::Enum(CoreCC, GuiTreeNodeFlagsCC);
};
}; // namespace ImGui
}; // namespace chainblocks
