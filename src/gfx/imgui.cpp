#include "imgui.hpp"
#include "frame.hpp"
#include "types.hpp"
#include "view.hpp"

#include <SDL2/SDL.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>
#include <bx/timer.h>
#include <imguizmo/ImGuizmo.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include "../../../content/shaders/imgui/fs_imgui_image.bin.h"
#include "../../../content/shaders/imgui/fs_ocornut_imgui.bin.h"
#include "../../../content/shaders/imgui/vs_imgui_image.bin.h"
#include "../../../content/shaders/imgui/vs_ocornut_imgui.bin.h"

#include "../../../content/fonts/icons_font_awesome.ttf.h"
#include "../../../content/fonts/icons_kenney.ttf.h"
#include "../../../content/fonts/roboto_regular.ttf.h"
#include "../../../content/fonts/robotomono_regular.ttf.h"

#include "fonts/icons_font_awesome.h"
#include "fonts/icons_kenney.h"

#define IMGUI_FLAGS_NONE UINT8_C(0x00)
#define IMGUI_FLAGS_ALPHA_BLEND UINT8_C(0x01)
#define IMGUI_MBUT_LEFT 0x01
#define IMGUI_MBUT_RIGHT 0x02
#define IMGUI_MBUT_MIDDLE 0x04

namespace gfx {

static const char *ImGui_ImplSDL2_GetClipboardText(ImguiContext *context) {
	if (context->_clipboardContents) {
		SDL_free(context->_clipboardContents);
	}
	context->_clipboardContents = SDL_GetClipboardText();
	return context->_clipboardContents;
}

static void ImGui_ImplSDL2_SetClipboardText(ImguiContext *context, const char *text) { SDL_SetClipboardText(text); }

static const bgfx::EmbeddedShader s_embeddedShaders[] = {BGFX_EMBEDDED_SHADER(vs_ocornut_imgui), BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
														 BGFX_EMBEDDED_SHADER(vs_imgui_image), BGFX_EMBEDDED_SHADER(fs_imgui_image),
														 BGFX_EMBEDDED_SHADER_END()};

struct FontRangeMerge {
	const void *data;
	size_t size;
	ImWchar ranges[3];
};

static FontRangeMerge s_fontRangeMerge[] = {{s_iconsKenneyTtf, sizeof(s_iconsKenneyTtf), {ICON_MIN_KI, ICON_MAX_KI, 0}},
											{s_iconsFontAwesomeTtf, sizeof(s_iconsFontAwesomeTtf), {ICON_MIN_FA, ICON_MAX_FA, 0}}};

bool ImguiContext::checkAvailTransientBuffers(uint32_t _numVertices, const bgfx::VertexLayout &_layout, uint32_t _numIndices) {
	return _numVertices == bgfx::getAvailTransientVertexBuffer(_numVertices, _layout) &&
		   (0 == _numIndices || _numIndices == bgfx::getAvailTransientIndexBuffer(_numIndices));
}

void ImguiContext::init(float _fontSize) {
	m_imgui = ::ImGui::CreateContext();

	ImGuiIO &io = ::ImGui::GetIO();

	// Keyboard mapping. ImGui will use those indices to peek into the
	// io.KeysDown[] array.
	io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
	io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
	io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
	io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
	io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
	io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
	io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
	io.KeyMap[ImGuiKey_KeyPadEnter] = SDL_SCANCODE_RETURN2;
	io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
	io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
	io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
	io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
	io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
	io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;

	io.ClipboardUserData = this;
	io.GetClipboardTextFn = (decltype(io.GetClipboardTextFn))ImGui_ImplSDL2_GetClipboardText;
	io.SetClipboardTextFn = (decltype(io.SetClipboardTextFn))ImGui_ImplSDL2_SetClipboardText;

	io.DisplaySize = ImVec2(1280.0f, 720.0f);
	io.DeltaTime = 1.0f / 60.0f;
	io.IniFilename = NULL;

	setupStyle(true);

	if (s_useCount == 0) {
		bgfx::RendererType::Enum type = bgfx::getRendererType();
		s_program = bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_ocornut_imgui"),
										bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_ocornut_imgui"), true);

		s_imageLodEnabled = bgfx::createUniform("u_imageLodEnabled", bgfx::UniformType::Vec4);
		s_imageProgram = bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_imgui_image"),
											 bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_imgui_image"), true);

		s_layout.begin()
			.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();

		s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

		uint8_t *data;
		int32_t width;
		int32_t height;
		{
			ImFontConfig config;
			config.FontDataOwnedByAtlas = false;
			config.MergeMode = false;
			//			config.MergeGlyphCenterV = true;

			const ImWchar *ranges = io.Fonts->GetGlyphRangesCyrillic();
			s_font[::ImGui::Font::Regular] = io.Fonts->AddFontFromMemoryTTF((void *)s_robotoRegularTtf, sizeof(s_robotoRegularTtf), _fontSize, &config, ranges);
			s_font[::ImGui::Font::Mono] =
				io.Fonts->AddFontFromMemoryTTF((void *)s_robotoMonoRegularTtf, sizeof(s_robotoMonoRegularTtf), _fontSize - 3.0f, &config, ranges);

			config.MergeMode = true;
			config.DstFont = s_font[::ImGui::Font::Regular];

			for (uint32_t ii = 0; ii < BX_COUNTOF(s_fontRangeMerge); ++ii) {
				const FontRangeMerge &frm = s_fontRangeMerge[ii];

				io.Fonts->AddFontFromMemoryTTF((void *)frm.data, (int)frm.size, _fontSize - 3.0f, &config, frm.ranges);
			}
		}

		io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);

		s_texture = bgfx::createTexture2D((uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::BGRA8, 0, bgfx::copy(data, width * height * 4));
	}

	s_useCount++;

	::ImGui::InitDockContext();
}

void ImguiContext::cleanup() {
	if (_clipboardContents) {
		SDL_free(_clipboardContents);
		_clipboardContents = nullptr;
	}

	::ImGui::ShutdownDockContext();
	::ImGui::DestroyContext(m_imgui);

	if (--s_useCount == 0) {
		bgfx::destroy(s_tex);
		bgfx::destroy(s_texture);
		bgfx::destroy(s_imageLodEnabled);
		bgfx::destroy(s_imageProgram);
		bgfx::destroy(s_program);
	}

	assert(s_useCount >= 0);
}

void ImguiContext::setupStyle(bool _dark) {
	// Doug Binks' darl color scheme
	// https://gist.github.com/dougbinks/8089b4bbaccaaf6fa204236978d165a9
	ImGuiStyle &style = ::ImGui::GetStyle();
	if (_dark) {
		::ImGui::StyleColorsDark(&style);
	} else {
		::ImGui::StyleColorsLight(&style);
	}

	style.FrameRounding = 4.0f;
	style.WindowBorderSize = 0.0f;
}

void ImguiContext::beginFrame(const gfx::View &mainView, const FrameInputs &frameInputs) {
	processInputEvents(frameInputs.events);

	int2 viewportSize = mainView.getSize();

	ImGuiIO &io = ::ImGui::GetIO();
	io.DisplaySize = ImVec2(float(viewportSize.x), float(viewportSize.y));
	io.DeltaTime = frameInputs.deltaTime;

	::ImGui::NewFrame();

	ImGuizmo::BeginFrame();

	lastMainView = &mainView;
}

void ImguiContext::endFrame(gfx::View &view) {
	::ImGui::Render();
	render(view, ::ImGui::GetDrawData());
	lastMainView = nullptr;
}

void ImguiContext::processInputEvents(const std::vector<SDL_Event> &events) {
	ImGuiIO &io = ::ImGui::GetIO();

	int32_t mouseX, mouseY;
	uint32_t mouseBtns = SDL_GetMouseState(&mouseX, &mouseY);
	uint8_t imButtons = 0;
	if (mouseBtns & SDL_BUTTON(SDL_BUTTON_LEFT)) {
		imButtons = imButtons | IMGUI_MBUT_LEFT;
	}
	if (mouseBtns & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
		imButtons = imButtons | IMGUI_MBUT_RIGHT;
	}
	if (mouseBtns & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
		imButtons = imButtons | IMGUI_MBUT_MIDDLE;
	}

	io.MouseWheel = 0.0f;
	io.MouseWheelH = 0.0f;

	for (const SDL_Event &event : events) {
		if (event.type == SDL_MOUSEWHEEL) {
			io.MouseWheel = float(event.wheel.y);
			io.MouseWheelH = float(event.wheel.x);
		} else if (event.type == SDL_MOUSEBUTTONDOWN) {
			// need to make sure to pass those as well or in low fps/simulated
			// clicks we might mess up
			if (event.button.button == SDL_BUTTON_LEFT) {
				imButtons = imButtons | IMGUI_MBUT_LEFT;
			} else if (event.button.button == SDL_BUTTON_RIGHT) {
				imButtons = imButtons | IMGUI_MBUT_RIGHT;
			} else if (event.button.button == SDL_BUTTON_MIDDLE) {
				imButtons = imButtons | IMGUI_MBUT_MIDDLE;
			}
		} else if (event.type == SDL_TEXTINPUT) {
			io.AddInputCharactersUTF8(event.text.text);
		} else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
			const auto key = event.key.keysym.scancode;
			const auto modState = SDL_GetModState();
			io.KeysDown[key] = event.type == SDL_KEYDOWN;
			io.KeyShift = ((modState & KMOD_SHIFT) != 0);
			io.KeyCtrl = ((modState & KMOD_CTRL) != 0);
			io.KeyAlt = ((modState & KMOD_ALT) != 0);
			io.KeySuper = ((modState & KMOD_GUI) != 0);
		}
	}

	io.MousePos = ImVec2(mouseX, mouseY);
	io.MouseDown[0] = 0 != (imButtons & IMGUI_MBUT_LEFT);
	io.MouseDown[1] = 0 != (imButtons & IMGUI_MBUT_RIGHT);
	io.MouseDown[2] = 0 != (imButtons & IMGUI_MBUT_MIDDLE);
}

void ImguiContext::render(gfx::View &view, ImDrawData *_drawData) {
	assert(lastMainView);

	const ImGuiIO &io = ::ImGui::GetIO();
	const float width = io.DisplaySize.x;
	const float height = io.DisplaySize.y;

	bgfx::setViewName(view.id, "ImGui");
	bgfx::setViewMode(view.id, bgfx::ViewMode::Sequential);

	const bgfx::Caps *caps = bgfx::getCaps();
	{
		float matrix[16];
		bx::mtxOrtho(matrix, 0.0f, width, height, 0.0f, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
		bgfx::setViewTransform(view.id, NULL, matrix);
		view.setViewport(lastMainView->getViewport());
	}

	// Render command lists
	for (int32_t ii = 0, num = _drawData->CmdListsCount; ii < num; ++ii) {
		bgfx::TransientVertexBuffer tvb;
		bgfx::TransientIndexBuffer tib;

		const ImDrawList *drawList = _drawData->CmdLists[ii];
		uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
		uint32_t numIndices = (uint32_t)drawList->IdxBuffer.size();

		if (!checkAvailTransientBuffers(numVertices, s_layout, numIndices)) {
			// not enough space in transient buffer just quit drawing the
			// rest...
			break;
		}

		bgfx::allocTransientVertexBuffer(&tvb, numVertices, s_layout);
		bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);

		ImDrawVert *verts = (ImDrawVert *)tvb.data;
		bx::memCopy(verts, drawList->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));

		ImDrawIdx *indices = (ImDrawIdx *)tib.data;
		bx::memCopy(indices, drawList->IdxBuffer.begin(), numIndices * sizeof(ImDrawIdx));

		uint32_t offset = 0;
		for (const ImDrawCmd *cmd = drawList->CmdBuffer.begin(), *cmdEnd = drawList->CmdBuffer.end(); cmd != cmdEnd; ++cmd) {
			if (cmd->UserCallback) {
				cmd->UserCallback(drawList, cmd);
			} else if (0 != cmd->ElemCount) {
				uint64_t state = 0 | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;

				bgfx::TextureHandle th = s_texture;
				bgfx::ProgramHandle program = s_program;

				if (NULL != cmd->TextureId) {
					union {
						ImTextureID ptr;
						struct {
							bgfx::TextureHandle handle;
							uint8_t flags;
							uint8_t mip;
						} s;
					} texture = {cmd->TextureId};
					state |= 0 != (IMGUI_FLAGS_ALPHA_BLEND & texture.s.flags)
								 ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
								 : BGFX_STATE_NONE;
					th = texture.s.handle;
					if (0 != texture.s.mip) {
						const float lodEnabled[4] = {float(texture.s.mip), 1.0f, 0.0f, 0.0f};
						bgfx::setUniform(s_imageLodEnabled, lodEnabled);
						program = s_imageProgram;
					}
				} else {
					state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
				}

				const uint16_t xx = uint16_t(bx::max(cmd->ClipRect.x, 0.0f));
				const uint16_t yy = uint16_t(bx::max(cmd->ClipRect.y, 0.0f));
				bgfx::setScissor(xx, yy, uint16_t(bx::min(cmd->ClipRect.z, 65535.0f) - xx), uint16_t(bx::min(cmd->ClipRect.w, 65535.0f) - yy));

				bgfx::setState(state);
				bgfx::setTexture(0, s_tex, th);
				bgfx::setVertexBuffer(0, &tvb, 0, numVertices);
				bgfx::setIndexBuffer(&tib, offset, cmd->ElemCount);
				bgfx::submit(view.id, program);
			}

			offset += cmd->ElemCount;
		}
	}
}

} // namespace gfx

namespace ImGui {
void PushFont(Font::Enum _font) { PushFont(gfx::ImguiContext::s_font[_font]); }
} // namespace ImGui
