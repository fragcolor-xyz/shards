/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

#include "chainblocks.hpp"
#include "imgui.h"

namespace ImGuiExtra {
#include "imgui_memory_editor.h"
};

namespace chainblocks {
namespace ImGui {
constexpr uint32_t ImGuiContextCC = 'ImGu';

struct Context {
  static inline TypeInfo Info = TypeInfo::Object(FragCC, ImGuiContextCC);
  // Useful to compare with with plugins, they might mismatch!
  static inline const char *Version = ::ImGui::GetVersion();

  ImGuiContext *context = ::ImGui::CreateContext();

  ~Context() { ::ImGui::DestroyContext(context); }

  void Set() { ::ImGui::SetCurrentContext(context); }

  void Reset() {
    ::ImGui::DestroyContext(context);
    context = ::ImGui::CreateContext();
  }
};
}; // namespace ImGui
}; // namespace chainblocks
