/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "imgui.h"
#include <SDL_events.h>
#include <vector>

namespace gfx {

struct FrameInputs;
struct View;
struct ImguiContext {
  void init(float _fontSize);
  void cleanup();

  bool checkAvailTransientBuffers(uint32_t _numVertices, const bgfx::VertexLayout &_layout, uint32_t _numIndices);
  void setupStyle(bool _dark);
  void beginFrame(const gfx::View &mainView, const FrameInputs &frameInputs);
  void endFrame(gfx::View &view);

private:
  void processInputEvents(const std::vector<SDL_Event> &events);
  void render(gfx::View &view, ImDrawData *_drawData);

public:
  const gfx::View *lastMainView;
  ImGuiContext *m_imgui;
  char *_clipboardContents = nullptr;

  static inline bgfx::VertexLayout s_layout;
  static inline bgfx::ProgramHandle s_program;
  static inline bgfx::ProgramHandle s_imageProgram;
  static inline bgfx::UniformHandle s_imageLodEnabled;
  static inline bgfx::TextureHandle s_texture;
  static inline bgfx::UniformHandle s_tex;
  static inline ImFont *s_font[::ImGui::Font::Count];
  static inline int s_useCount{0};
  static inline const char *Version = ::ImGui::GetVersion();
};
} // namespace gfx

namespace ImGui {
void PushFont(Font::Enum _font);
} // namespace ImGui
