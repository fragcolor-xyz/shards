/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <bgfx/bgfx.h>
#include <chainblocks.hpp>
#include <SDL2/SDL_events.h>
#include "imgui.h"

namespace gfx {

struct FrameInputs;
struct ViewInfo;
struct ImguiContext {
	void init(float _fontSize, bgfx::ViewId _viewId);
	void cleanup();

	bool checkAvailTransientBuffers(uint32_t _numVertices, const bgfx::VertexLayout &_layout, uint32_t _numIndices);
	void setupStyle(bool _dark);
	void beginFrame(const gfx::ViewInfo &mainView, const FrameInputs &frameInputs);
	void endFrame();

private:
	void processInputEvents(const std::vector<SDL_Event> &events);
	void render(ImDrawData *_drawData);

public:
	ImGuiContext *m_imgui;
	bgfx::ViewId m_viewId;
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
