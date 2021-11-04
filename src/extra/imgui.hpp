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
#define REGISTER_FLAGS_EX(_NAME_, _CC_)                                        \
  REGISTER_FLAGS(_NAME_, _CC_);                                                \
  static inline Type _NAME_##SeqType = Type::SeqOf(_NAME_##Type);              \
  static inline Type _NAME_##VarType = Type::VariableOf(_NAME_##Type);         \
  static inline Type _NAME_##VarSeqType = Type::VariableOf(_NAME_##SeqType)

  enum class GuiButton {
    Normal,
    Small,
    Invisible,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown
  };
  REGISTER_ENUM(GuiButton, 'guiB'); // FourCC = 0x67756942

  enum class GuiTableFlags {
    None = ::ImGuiTableFlags_None,
    Resizable = ::ImGuiTableFlags_Resizable,
    Reorderable = ::ImGuiTableFlags_Reorderable,
    Hideable = ::ImGuiTableFlags_Hideable,
    Sortable = ::ImGuiTableFlags_Sortable
  };
  REGISTER_FLAGS_EX(GuiTableFlags, 'guiT'); // FourCC = 0x67756954

  enum class GuiTableColumnFlags {
    None = ::ImGuiTableColumnFlags_None,
    Disabled = ::ImGuiTableColumnFlags_Disabled,
    DefaultHide = ::ImGuiTableColumnFlags_DefaultHide,
    DefaultSort = ::ImGuiTableColumnFlags_DefaultSort
  };
  REGISTER_FLAGS_EX(GuiTableColumnFlags, 'guTC'); // FourCC = 0x67755443

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
  REGISTER_FLAGS_EX(GuiTreeNodeFlags, 'guTN'); // FourCC = 0x6775544E
};
}; // namespace ImGui
}; // namespace chainblocks
