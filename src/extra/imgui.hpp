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
  enum class GuiDir { Left, Right, Up, Down };
  static constexpr uint32_t GuiDirCC = 'guiD';
  static inline EnumInfo<GuiDir> GuiDirEnumInfo{"GuiDir", CoreCC, GuiDirCC};
  static inline Type GuiDirType = Type::Enum(CoreCC, GuiDirCC);

  static constexpr ::ImGuiDir DirToImGui(CBEnum eval) {
    switch (GuiDir(eval)) {
    case GuiDir::Left:
      return ::ImGuiDir_Left;
    case GuiDir::Right:
      return ::ImGuiDir_Right;
    case GuiDir::Up:
      return ::ImGuiDir_Up;
    case GuiDir::Down:
      return ::ImGuiDir_Down;
    }
    return ::ImGuiDir_None;
  }
};
}; // namespace ImGui
}; // namespace chainblocks
