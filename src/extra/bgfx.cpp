/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "./bgfx.hpp"
#include "./imgui.hpp"
#include "SDL.h"
#include <bgfx/embedded_shader.h>
#include <bx/debug.h>
#include <bx/math.h>
#include <bx/timer.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stb_image_write.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

using namespace chainblocks;

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include "fs_imgui_image.bin.h"
#include "fs_ocornut_imgui.bin.h"
#include "vs_imgui_image.bin.h"
#include "vs_ocornut_imgui.bin.h"

#include "icons_font_awesome.ttf.h"
#include "icons_kenney.ttf.h"
#include "roboto_regular.ttf.h"
#include "robotomono_regular.ttf.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(vs_imgui_image), BGFX_EMBEDDED_SHADER(fs_imgui_image),
    BGFX_EMBEDDED_SHADER_END()};

struct FontRangeMerge {
  const void *data;
  size_t size;
  ImWchar ranges[3];
};

static FontRangeMerge s_fontRangeMerge[] = {
    {s_iconsKenneyTtf, sizeof(s_iconsKenneyTtf), {ICON_MIN_KI, ICON_MAX_KI, 0}},
    {s_iconsFontAwesomeTtf,
     sizeof(s_iconsFontAwesomeTtf),
     {ICON_MIN_FA, ICON_MAX_FA, 0}}};

struct OcornutImguiContext {
#define IMGUI_FLAGS_NONE UINT8_C(0x00)
#define IMGUI_FLAGS_ALPHA_BLEND UINT8_C(0x01)
#define IMGUI_MBUT_LEFT 0x01
#define IMGUI_MBUT_RIGHT 0x02
#define IMGUI_MBUT_MIDDLE 0x04

  bool checkAvailTransientBuffers(uint32_t _numVertices,
                                  const bgfx::VertexLayout &_layout,
                                  uint32_t _numIndices) {
    return _numVertices ==
               bgfx::getAvailTransientVertexBuffer(_numVertices, _layout) &&
           (0 == _numIndices ||
            _numIndices == bgfx::getAvailTransientIndexBuffer(_numIndices));
  }

  void render(ImDrawData *_drawData) {
    const ImGuiIO &io = ::ImGui::GetIO();
    const float width = io.DisplaySize.x;
    const float height = io.DisplaySize.y;

    bgfx::setViewName(m_viewId, "ImGui");
    bgfx::setViewMode(m_viewId, bgfx::ViewMode::Sequential);

    const bgfx::Caps *caps = bgfx::getCaps();
    {
      float ortho[16];
      bx::mtxOrtho(ortho, 0.0f, width, height, 0.0f, 0.0f, 1000.0f, 0.0f,
                   caps->homogeneousDepth);
      bgfx::setViewTransform(m_viewId, NULL, ortho);
      bgfx::setViewRect(m_viewId, 0, 0, uint16_t(width), uint16_t(height));
    }

    // Render command lists
    for (int32_t ii = 0, num = _drawData->CmdListsCount; ii < num; ++ii) {
      bgfx::TransientVertexBuffer tvb;
      bgfx::TransientIndexBuffer tib;

      const ImDrawList *drawList = _drawData->CmdLists[ii];
      uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
      uint32_t numIndices = (uint32_t)drawList->IdxBuffer.size();

      if (!checkAvailTransientBuffers(numVertices, s_layout, numIndices)) {
        // not enough space in transient buffer just quit drawing the rest...
        break;
      }

      bgfx::allocTransientVertexBuffer(&tvb, numVertices, s_layout);
      bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);

      ImDrawVert *verts = (ImDrawVert *)tvb.data;
      bx::memCopy(verts, drawList->VtxBuffer.begin(),
                  numVertices * sizeof(ImDrawVert));

      ImDrawIdx *indices = (ImDrawIdx *)tib.data;
      bx::memCopy(indices, drawList->IdxBuffer.begin(),
                  numIndices * sizeof(ImDrawIdx));

      uint32_t offset = 0;
      for (const ImDrawCmd *cmd = drawList->CmdBuffer.begin(),
                           *cmdEnd = drawList->CmdBuffer.end();
           cmd != cmdEnd; ++cmd) {
        if (cmd->UserCallback) {
          cmd->UserCallback(drawList, cmd);
        } else if (0 != cmd->ElemCount) {
          uint64_t state =
              0 | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;

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
                         ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                 BGFX_STATE_BLEND_INV_SRC_ALPHA)
                         : BGFX_STATE_NONE;
            th = texture.s.handle;
            if (0 != texture.s.mip) {
              const float lodEnabled[4] = {float(texture.s.mip), 1.0f, 0.0f,
                                           0.0f};
              bgfx::setUniform(s_imageLodEnabled, lodEnabled);
              program = s_imageProgram;
            }
          } else {
            state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);
          }

          const uint16_t xx = uint16_t(bx::max(cmd->ClipRect.x, 0.0f));
          const uint16_t yy = uint16_t(bx::max(cmd->ClipRect.y, 0.0f));
          bgfx::setScissor(xx, yy,
                           uint16_t(bx::min(cmd->ClipRect.z, 65535.0f) - xx),
                           uint16_t(bx::min(cmd->ClipRect.w, 65535.0f) - yy));

          bgfx::setState(state);
          bgfx::setTexture(0, s_tex, th);
          bgfx::setVertexBuffer(0, &tvb, 0, numVertices);
          bgfx::setIndexBuffer(&tib, offset, cmd->ElemCount);
          bgfx::submit(m_viewId, program);
        }

        offset += cmd->ElemCount;
      }
    }
  }

  void create(float _fontSize, bgfx::ViewId _viewId) {
    m_viewId = _viewId;
    m_lastScroll = 0;
    m_last = bx::getHPCounter();

    m_imgui = ::ImGui::CreateContext();

    ImGuiIO &io = ::ImGui::GetIO();

    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = NULL;

    setupStyle(true);

    if (s_useCount == 0) {
      bgfx::RendererType::Enum type = bgfx::getRendererType();
      s_program =
          bgfx::createProgram(bgfx::createEmbeddedShader(
                                  s_embeddedShaders, type, "vs_ocornut_imgui"),
                              bgfx::createEmbeddedShader(
                                  s_embeddedShaders, type, "fs_ocornut_imgui"),
                              true);

      s_imageLodEnabled =
          bgfx::createUniform("u_imageLodEnabled", bgfx::UniformType::Vec4);
      s_imageProgram = bgfx::createProgram(
          bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_imgui_image"),
          bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_imgui_image"),
          true);

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
        s_font[::ImGui::Font::Regular] = io.Fonts->AddFontFromMemoryTTF(
            (void *)s_robotoRegularTtf, sizeof(s_robotoRegularTtf), _fontSize,
            &config, ranges);
        s_font[::ImGui::Font::Mono] = io.Fonts->AddFontFromMemoryTTF(
            (void *)s_robotoMonoRegularTtf, sizeof(s_robotoMonoRegularTtf),
            _fontSize - 3.0f, &config, ranges);

        config.MergeMode = true;
        config.DstFont = s_font[::ImGui::Font::Regular];

        for (uint32_t ii = 0; ii < BX_COUNTOF(s_fontRangeMerge); ++ii) {
          const FontRangeMerge &frm = s_fontRangeMerge[ii];

          io.Fonts->AddFontFromMemoryTTF((void *)frm.data, (int)frm.size,
                                         _fontSize - 3.0f, &config, frm.ranges);
        }
      }

      io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);

      s_texture = bgfx::createTexture2D((uint16_t)width, (uint16_t)height,
                                        false, 1, bgfx::TextureFormat::BGRA8, 0,
                                        bgfx::copy(data, width * height * 4));
    }

    s_useCount++;

    ::ImGui::InitDockContext();
  }

  void destroy() {
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

  void setupStyle(bool _dark) {
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

  void beginFrame(int32_t _mx, int32_t _my, uint8_t _button, int32_t _scroll,
                  int _width, int _height) {
    ImGuiIO &io = ::ImGui::GetIO();

    io.DisplaySize = ImVec2((float)_width, (float)_height);

    const int64_t now = bx::getHPCounter();
    const int64_t frameTime = now - m_last;
    m_last = now;
    const double freq = double(bx::getHPFrequency());
    io.DeltaTime = float(frameTime / freq);

    io.MousePos = ImVec2((float)_mx, (float)_my);
    io.MouseDown[0] = 0 != (_button & IMGUI_MBUT_LEFT);
    io.MouseDown[1] = 0 != (_button & IMGUI_MBUT_RIGHT);
    io.MouseDown[2] = 0 != (_button & IMGUI_MBUT_MIDDLE);
    io.MouseWheel = (float)(_scroll - m_lastScroll);
    m_lastScroll = _scroll;

    ::ImGui::NewFrame();

    ImGuizmo::BeginFrame();
  }

  void endFrame() {
    ::ImGui::Render();
    render(::ImGui::GetDrawData());
  }

  ImGuiContext *m_imgui;
  int64_t m_last;
  int32_t m_lastScroll;
  bgfx::ViewId m_viewId;

  static inline bgfx::VertexLayout s_layout;
  static inline bgfx::ProgramHandle s_program;
  static inline bgfx::ProgramHandle s_imageProgram;
  static inline bgfx::UniformHandle s_imageLodEnabled;
  static inline bgfx::TextureHandle s_texture;
  static inline bgfx::UniformHandle s_tex;
  static inline ImFont *s_font[::ImGui::Font::Count];
  static inline int s_useCount{0};
};

namespace ImGui {
void PushFont(Font::Enum _font) {
  PushFont(OcornutImguiContext::s_font[_font]);
}
} // namespace ImGui

namespace BGFX {
void *SDL_GetNativeWindowPtr(SDL_Window *window);
void *SDL_GetNativeDisplayPtr(SDL_Window *window);

// DPI awareness fix
#ifdef _WIN32
struct DpiAwareness {
  DpiAwareness() { SetProcessDPIAware(); }
};
#endif

struct BaseWindow : public Base {
#ifdef _WIN32
  static inline DpiAwareness DpiAware{};
#elif defined(__APPLE__)
  SDL_MetalView _metalView{nullptr};
#endif

  void* _sysWnd = nullptr;

  static inline Parameters params{
      {"Title",
       CBCCSTR("The title of the window to create."),
       {CoreInfo::StringType}},
      {"Width",
       CBCCSTR("The width of the window to create. In pixels and DPI aware."),
       {CoreInfo::IntType}},
      {"Height",
       CBCCSTR("The height of the window to create. In pixels and DPI aware."),
       {CoreInfo::IntType}},
      {"Contents",
       CBCCSTR("The contents of this window."),
       {CoreInfo::BlocksOrNone}},
      {"Fullscreen",
       CBCCSTR("If the window should use fullscreen mode."),
       {CoreInfo::BoolType}},
      {"Debug",
       CBCCSTR("If the device backing the window should be created with "
               "debug layers on."),
       {CoreInfo::BoolType}},
      {"ClearColor",
       CBCCSTR("The color to use to clear the backbuffer at the beginning of a "
               "new frame."),
       {CoreInfo::ColorType}}};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return params; }

  std::string _title;
  int _pwidth = 1024;
  int _pheight = 768;
  int _rwidth = 1024;
  int _rheight = 768;
  bool _debug = false;
  bool _fsMode{false};
  SDL_Window *_window = nullptr;
  CBVar *_sdlWinVar = nullptr;
  CBVar *_imguiCtx = nullptr;
  CBVar *_nativeWnd = nullptr;
  BlocksVar _blocks;
  OcornutImguiContext _imguiBgfxCtx;
  CBColor _clearColor{0xC0, 0xC0, 0xAA, 0xFF};

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      break;
    case 1:
      _pwidth = int(value.payload.intValue);
      break;
    case 2:
      _pheight = int(value.payload.intValue);
      break;
    case 3:
      _blocks = value;
      break;
    case 4:
      _fsMode = value.payload.boolValue;
      break;
    case 5:
      _debug = value.payload.boolValue;
      break;
    case 6:
      _clearColor = value.payload.colorValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_title);
    case 1:
      return Var(_pwidth);
    case 2:
      return Var(_pheight);
    case 3:
      return _blocks;
    case 4:
      return Var(_fsMode);
    case 5:
      return Var(_debug);
    case 6:
      return Var(_clearColor);
    default:
      return Var::Empty;
    }
  }
};

struct MainWindow : public BaseWindow {
  struct CallbackStub : public bgfx::CallbackI {
    virtual ~CallbackStub() {}

    virtual void fatal(const char *_filePath, uint16_t _line,
                       bgfx::Fatal::Enum _code, const char *_str) override {
      if (bgfx::Fatal::DebugCheck == _code) {
        bx::debugBreak();
      } else {
        fatalError = _str;
        throw std::runtime_error(_str);
      }
    }

    virtual void traceVargs(const char *_filePath, uint16_t _line,
                            const char *_format, va_list _argList) override {
      char temp[2048];
      char *out = temp;
      va_list argListCopy;
      va_copy(argListCopy, _argList);
      int32_t len =
          bx::snprintf(out, sizeof(temp), "%s (%d): ", _filePath, _line);
      int32_t total = len + bx::vsnprintf(out + len, sizeof(temp) - len,
                                          _format, argListCopy);
      va_end(argListCopy);
      if ((int32_t)sizeof(temp) < total) {
        out = (char *)alloca(total + 1);
        bx::memCopy(out, temp, len);
        bx::vsnprintf(out + len, total - len, _format, _argList);
      }
      out[total] = '\0';
      CBLOG_DEBUG("(bgfx): {}", out);
    }

    virtual void profilerBegin(const char * /*_name*/, uint32_t /*_abgr*/,
                               const char * /*_filePath*/,
                               uint16_t /*_line*/) override {}

    virtual void profilerBeginLiteral(const char * /*_name*/,
                                      uint32_t /*_abgr*/,
                                      const char * /*_filePath*/,
                                      uint16_t /*_line*/) override {}

    virtual void profilerEnd() override {}

    virtual uint32_t cacheReadSize(uint64_t /*_id*/) override { return 0; }

    virtual bool cacheRead(uint64_t /*_id*/, void * /*_data*/,
                           uint32_t /*_size*/) override {
      return false;
    }

    virtual void cacheWrite(uint64_t /*_id*/, const void * /*_data*/,
                            uint32_t /*_size*/) override {}

    virtual void screenShot(const char *_filePath, uint32_t _width,
                            uint32_t _height, uint32_t _pitch,
                            const void *_data, uint32_t _size,
                            bool _yflip) override {
      CBLOG_DEBUG("Screenshot requested for path:  {}", _filePath);

      // For now.. TODO as might be not true
      assert(_pitch == (_width * 4));

      // need to convert to rgba
      std::vector<uint8_t> data;
      data.resize(_size);
      const uint8_t *bgra = (uint8_t *)_data;
      for (uint32_t x = 0; x < _height; x++) {
        for (uint32_t y = 0; y < _width; y++) {
          data[((y * 4) + 0) + (_pitch * x)] =
              bgra[((y * 4) + 2) + (_pitch * x)];
          data[((y * 4) + 1) + (_pitch * x)] =
              bgra[((y * 4) + 1) + (_pitch * x)];
          data[((y * 4) + 2) + (_pitch * x)] =
              bgra[((y * 4) + 0) + (_pitch * x)];
          data[((y * 4) + 3) + (_pitch * x)] =
              bgra[((y * 4) + 3) + (_pitch * x)];
        }
      }

      stbi_flip_vertically_on_write(_yflip);
      std::filesystem::path p(_filePath);
      const auto ext = p.extension();
      if (ext == ".png" || ext == ".PNG") {
        stbi_write_png(_filePath, _width, _height, 4, data.data(), _pitch);
      } else if (ext == ".jpg" || ext == ".JPG" || ext == ".jpeg" ||
                 ext == ".JPEG") {
        stbi_write_jpg(_filePath, _width, _height, 4, data.data(), 95);
      }
    }

    virtual void captureBegin(uint32_t /*_width*/, uint32_t /*_height*/,
                              uint32_t /*_pitch*/,
                              bgfx::TextureFormat::Enum /*_format*/,
                              bool /*_yflip*/) override {
      BX_TRACE("Warning: using capture without callback (a.k.a. pointless).");
    }

    virtual void captureEnd() override {}

    virtual void captureFrame(const void * /*_data*/,
                              uint32_t /*_size*/) override {}

    std::string fatalError;
  } bgfxCallback;

  Context _bgfxContext{};
  chainblocks::ImGui::Context _imguiContext{};
  int32_t _wheelScroll = 0;
  bool _bgfxInit{false};
  float _windowScalingW{1.0};
  float _windowScalingH{1.0};
  bgfx::UniformHandle _timeUniformHandle = BGFX_INVALID_HANDLE;

  CBTypeInfo compose(CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("GFX Blocks cannot be used on a worker thread (e.g. "
                         "within an Await block)");
    }

    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
        throw CBException("GFX.MainWindow must be unique, found another use!");
      }
    }

    // twice to actually own the data and release...
    IterableExposedInfo rshared(data.shared);
    IterableExposedInfo shared(rshared);
    shared.push_back(ExposedInfo::ProtectedVariable(
        "GFX.CurrentWindow", CBCCSTR("The exposed SDL window."),
        BaseConsumer::windowType));
    shared.push_back(ExposedInfo::ProtectedVariable(
        "GFX.Context", CBCCSTR("The BGFX Context."), Context::Info));
    shared.push_back(ExposedInfo::ProtectedVariable(
        "GUI.Context", CBCCSTR("The ImGui Context."),
        chainblocks::ImGui::Context::Info));
    data.shared = shared;
    _blocks.compose(data);

    return data.inputType;
  }

  void cleanup() {
    // cleanup before releasing the resources
    _blocks.cleanup();

    // _imguiContext.Reset();
    _bgfxContext.reset();

    if (_bgfxInit) {
      _imguiBgfxCtx.destroy();
      bgfx::shutdown();
    }

    unregisterRunLoopCallback("fragcolor.gfx.ospump");

#ifdef __APPLE__
    if (_metalView) {
      SDL_Metal_DestroyView(_metalView);
      _metalView = nullptr;
    }
#endif

    if (_window) {
      SDL_DestroyWindow(_window);
      SDL_Quit();
      _window = nullptr;
      _sysWnd = nullptr;
    }

    if (_sdlWinVar) {
      if (_sdlWinVar->refcount > 1) {
        CBLOG_ERROR(
            "MainWindow: Found a dangling reference to GFX.CurrentWindow");
      }
      memset(_sdlWinVar, 0x0, sizeof(CBVar));
      _sdlWinVar = nullptr;
    }

    if (_bgfxCtx) {
      if (_bgfxCtx->refcount > 1) {
        CBLOG_ERROR("MainWindow: Found a dangling reference to GFX.Context");
      }
      memset(_bgfxCtx, 0x0, sizeof(CBVar));
      _bgfxCtx = nullptr;
    }

    if (_imguiCtx) {
      if (_imguiCtx->refcount > 1) {
        CBLOG_ERROR("MainWindow: Found a dangling reference to GUI.Context");
      }
      memset(_imguiCtx, 0x0, sizeof(CBVar));
      _imguiCtx = nullptr;
    }

    if (_nativeWnd) {
      releaseVariable(_nativeWnd);
      _nativeWnd = nullptr;
    }

    _wheelScroll = 0;
    _bgfxInit = false;
  }

  static inline char *_clipboardContents{nullptr};

  static const char *ImGui_ImplSDL2_GetClipboardText(void *) {
    if (_clipboardContents)
      SDL_free(_clipboardContents);
    _clipboardContents = SDL_GetClipboardText();
    return _clipboardContents;
  }

  static void ImGui_ImplSDL2_SetClipboardText(void *, const char *text) {
    SDL_SetClipboardText(text);
  }

  constexpr static uint32_t BgfxFlags = BGFX_RESET_VSYNC;

  struct ProcessClock {
    decltype(std::chrono::high_resolution_clock::now()) Start;
    ProcessClock() { Start = std::chrono::high_resolution_clock::now(); }
  };

  ProcessClock _absTimer;
  ProcessClock _deltaTimer;
  uint32_t _frameCounter{0};

  void warmup(CBContext *context) {
    // do not touch parameter values
    _rwidth = _pwidth;
    _rheight = _pheight;

    auto initErr = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
    if (initErr != 0) {
      CBLOG_ERROR("Failed to initialize SDL {}", SDL_GetError());
      throw ActivationError("Failed to initialize SDL");
    }

    registerRunLoopCallback("fragcolor.gfx.ospump", [] {
      Context::sdlEvents.clear();
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        Context::sdlEvents.push_back(event);
      }
    });

    bgfx::Init initInfo{};
    initInfo.callback = &bgfxCallback;

    // try to see if global native window is set
    _nativeWnd = referenceVariable(context, "fragcolor.gfx.nativewindow");
    if (_nativeWnd->valueType == Object &&
        _nativeWnd->payload.objectVendorId == CoreCC &&
        _nativeWnd->payload.objectTypeId == BgfxNativeWindowCC) {
      _sysWnd = decltype(_sysWnd)(_nativeWnd->payload.objectValue);
      // TODO SDL_CreateWindowFrom to enable inputs etc...
      // Urgent TODO if iOS is needed
      // specially for iOS thing is that we pass context as variable, not a
      // window object we might need 2 variables in the end
    } else {
      uint32_t flags = SDL_WINDOW_SHOWN |
#ifndef __EMSCRIPTEN__
                       SDL_WINDOW_ALLOW_HIGHDPI |
#endif
                       SDL_WINDOW_RESIZABLE |
                       (_fsMode ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#ifdef __APPLE__
      flags |= SDL_WINDOW_METAL;
#endif
      // TODO: SDL_WINDOW_BORDERLESS
      _window = SDL_CreateWindow(_title.c_str(), SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, _fsMode ? 0 : _rwidth,
                                 _fsMode ? 0 : _rheight, flags);

      // get the window size again to ensure it's correct
      SDL_GetWindowSize(_window, &_rwidth, &_rheight);

      CBLOG_DEBUG("GFX.MainWindow width: {} height: {}", _rwidth, _rheight);

#ifdef __APPLE__
      _metalView = SDL_Metal_CreateView(_window);
      if (!_fsMode) {
        // seems to work only when windowed...
        // tricky cos retina..
        // find out the scaling
        int real_w, real_h;
        SDL_Metal_GetDrawableSize(_window, &real_w, &real_h);
        _windowScalingW = float(real_w) / float(_rwidth);
        _windowScalingH = float(real_h) / float(_rheight);
        // fix the scaling now if needed
        if (_windowScalingW != 1.0 || _windowScalingH != 1.0) {
          SDL_Metal_DestroyView(_metalView);
          SDL_SetWindowSize(_window, int(float(_rwidth) / _windowScalingW),
                            int(float(_rheight) / _windowScalingH));
          _metalView = SDL_Metal_CreateView(_window);
        }
      } else {
        int real_w, real_h;
        SDL_Metal_GetDrawableSize(_window, &real_w, &real_h);
        _windowScalingW = float(real_w) / float(_rwidth);
        _windowScalingH = float(real_h) / float(_rheight);
        // finally in this case override our real values
        _rwidth = real_w;
        _rheight = real_h;
      }
      _sysWnd = (void*)SDL_Metal_GetLayer(_metalView);
#elif defined(_WIN32) || defined(__linux__)
      _sysWnd = (void*)SDL_GetNativeWindowPtr(_window);
#elif defined(__EMSCRIPTEN__)
      _sysWnd = (void *)("#canvas"); // SDL and emscripten use #canvas
#endif
    }

    _nativeWnd->valueType = Object;
    _nativeWnd->payload.objectValue = _sysWnd;
    _nativeWnd->payload.objectVendorId = CoreCC;
    _nativeWnd->payload.objectTypeId = BgfxNativeWindowCC;

    // Ensure clicks will happen even from out of focus!
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    // set platform data this way.. or we will have issues if we re-init bgfx
    bgfx::PlatformData pdata{};
    pdata.nwh = _sysWnd;
    pdata.ndt = SDL_GetNativeDisplayPtr(_window);
    bgfx::setPlatformData(pdata);

    initInfo.resolution.width = _rwidth;
    initInfo.resolution.height = _rheight;
    initInfo.resolution.reset = BgfxFlags;
    initInfo.debug = _debug;
    if (!bgfx::init(initInfo)) {
      throw ActivationError("Failed to initialize BGFX");
    } else {
      _bgfxInit = true;
    }

#ifdef BGFX_CONFIG_RENDERER_OPENGL_MIN_VERSION
    CBLOG_INFO("Renderer version: {}",
               bgfx::getRendererName(bgfx::RendererType::OpenGL));
#elif BGFX_CONFIG_RENDERER_OPENGLES_MIN_VERSION
    CBLOG_INFO("Renderer version: {}",
               bgfx::getRendererName(bgfx::RendererType::OpenGLES));
#endif

    // _imguiContext.Reset();
    // _imguiContext.Set();

    _imguiBgfxCtx.create(18.0, 255);

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

    io.GetClipboardTextFn = ImGui_ImplSDL2_GetClipboardText;
    io.SetClipboardTextFn = ImGui_ImplSDL2_SetClipboardText;

    bgfx::setViewRect(0, 0, 0, _rwidth, _rheight);
    bgfx::setViewClear(0,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                       Var(_clearColor).colorToInt(), 1.0f, 0);

    _sdlWinVar = referenceVariable(context, "GFX.CurrentWindow");
    _bgfxCtx = referenceVariable(context, "GFX.Context");
    _imguiCtx = referenceVariable(context, "GUI.Context");

    // populate them here too to allow warmup operation
    _sdlWinVar->valueType = _bgfxCtx->valueType = _imguiCtx->valueType =
        CBType::Object;
    _sdlWinVar->payload.objectVendorId = _bgfxCtx->payload.objectVendorId =
        _imguiCtx->payload.objectVendorId = CoreCC;

    _sdlWinVar->payload.objectTypeId = windowCC;
    _sdlWinVar->payload.objectValue = _window;

    _bgfxCtx->payload.objectTypeId = BgfxContextCC;
    _bgfxCtx->payload.objectValue = &_bgfxContext;

    _imguiCtx->payload.objectTypeId = chainblocks::ImGui::ImGuiContextCC;
    _imguiCtx->payload.objectValue = &_imguiContext;

    auto viewId = _bgfxContext.nextViewId();
    assert(viewId == 0); // always 0 in MainWindow

    // create time uniform
    _timeUniformHandle =
        bgfx::createUniform("u_private_time4", bgfx::UniformType::Vec4, 1);
    _deltaTimer.Start = std::chrono::high_resolution_clock::now();

    // reset frame counter
    _frameCounter = 0;

    // init blocks after we initialize the system
    _blocks.warmup(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!bgfxCallback.fatalError.empty()) {
      throw ActivationError(bgfxCallback.fatalError);
    }

    // Deal with inputs first as we might resize etc

    // _imguiContext.Set();

    ImGuiIO &io = ::ImGui::GetIO();

    int32_t mouseX, mouseY;
    uint32_t mouseBtns = SDL_GetMouseState(&mouseX, &mouseY);
    uint8_t imbtns = 0;
    if (mouseBtns & SDL_BUTTON(SDL_BUTTON_LEFT)) {
      imbtns = imbtns | IMGUI_MBUT_LEFT;
    }
    if (mouseBtns & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
      imbtns = imbtns | IMGUI_MBUT_RIGHT;
    }
    if (mouseBtns & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
      imbtns = imbtns | IMGUI_MBUT_MIDDLE;
    }

    // process some events
    for (auto &event : Context::sdlEvents) {
      if (event.type == SDL_MOUSEWHEEL) {
        _wheelScroll += event.wheel.y;
        // This is not needed seems.. not even on MacOS Natural On/Off
        // if (event.wheel.direction == SDL_MOUSEWHEEL_NORMAL)
        //   _wheelScroll += event.wheel.y;
        // else
        //   _wheelScroll -= event.wheel.y;
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        // need to make sure to pass those as well or in low fps/simulated
        // clicks we might mess up
        if (event.button.button == SDL_BUTTON_LEFT) {
          imbtns = imbtns | IMGUI_MBUT_LEFT;
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
          imbtns = imbtns | IMGUI_MBUT_RIGHT;
        } else if (event.button.button == SDL_BUTTON_MIDDLE) {
          imbtns = imbtns | IMGUI_MBUT_MIDDLE;
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
      } else if (event.type == SDL_QUIT) {
        // stop the current chain on close
        throw ActivationError("Window closed, aborting chain.");
      } else if (event.type == SDL_WINDOWEVENT &&
                 SDL_GetWindowID(_window) ==
                     event.window
                         .windowID) { // support multiple windows closure
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
          // stop the current chain on close
          throw ActivationError("Window closed, aborting chain.");
        } else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          int w, h;
          SDL_GetWindowSize(_window, &w, &h);
          CBLOG_DEBUG("GFX.MainWindow size changed width: {} height: {}", w, h);
        } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          // get the window size again to ensure it's correct
          SDL_GetWindowSize(_window, &_rwidth, &_rheight);

          CBLOG_DEBUG("GFX.MainWindow resized width: {} height: {}", _rwidth,
                      _rheight);
#ifdef __APPLE__
          int real_w, real_h;
          SDL_Metal_GetDrawableSize(_window, &real_w, &real_h);
          _windowScalingW = float(real_w) / float(_rwidth);
          _windowScalingH = float(real_h) / float(_rheight);
          // finally in this case override our real values
          _rwidth = real_w;
          _rheight = real_h;
#endif
          bgfx::reset(_rwidth, _rheight, BgfxFlags);
        }
      }
    }

#ifdef __EMSCRIPTEN__
    double canvasWidth, canvasHeight;
    EMSCRIPTEN_RESULT res =
        emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);
    if (res != EMSCRIPTEN_RESULT_SUCCESS) {
      throw ActivationError("Failed to callemscripten_get_element_css_size");
    }
    int canvasW = int(canvasWidth);
    int canvasH = int(canvasHeight);
    if (canvasW != _rwidth || canvasH != _rheight) {
      CBLOG_DEBUG("GFX.MainWindow canvas size changed width: {} height: {}",
                  canvasW, canvasH);
      SDL_SetWindowSize(_window, canvasW, canvasH);
      _rwidth = canvasW;
      _rheight = canvasH;
      bgfx::reset(_rwidth, _rheight, BgfxFlags);
    }
#endif

    // now that we settled inputs and possible resize push GUI

    // push view 0
    _bgfxContext.pushView({0, _rwidth, _rheight, BGFX_INVALID_HANDLE});
    DEFER({
      _bgfxContext.popView();
      assert(_bgfxContext.viewIndex() == 0);
    });

    // Touch view 0
    bgfx::touch(0);

    // Set time
    const auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> ddt = tnow - _deltaTimer.Start;
    _deltaTimer.Start = tnow; // reset timer
    std::chrono::duration<float> adt = tnow - _absTimer.Start;
    float time[4] = {ddt.count(), adt.count(), float(_frameCounter++), 0.0};
    bgfx::setUniform(_timeUniformHandle, time, 1);

    if (_windowScalingW != 1.0 || _windowScalingH != 1.0) {
      mouseX = int32_t(float(mouseX) * _windowScalingW);
      mouseY = int32_t(float(mouseY) * _windowScalingH);
    }

    _imguiBgfxCtx.beginFrame(mouseX, mouseY, imbtns, _wheelScroll, // mouse
                             _rwidth, _rheight);

    // activate the blocks and render
    CBVar output{};
    _blocks.activate(context, input, output);

    // finish up with this frame
    _imguiBgfxCtx.endFrame();
    bgfx::frame();

    return input;
  }
};

struct Texture2D : public BaseConsumer {
  Texture *_texture{nullptr};

  void cleanup() {
    if (_texture) {
      Texture::Var.Release(_texture);
      _texture = nullptr;
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }

  static CBTypesInfo outputTypes() { return Texture::ObjType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);
    return Texture::ObjType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto bpp = 1;
    if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      bpp = 2;
    else if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      bpp = 4;

    // Upload a completely new image if sizes changed, also first activation!
    if (!_texture || input.payload.imageValue.width != _texture->width ||
        input.payload.imageValue.height != _texture->height ||
        input.payload.imageValue.channels != _texture->channels ||
        bpp != _texture->bpp) {
      if (_texture) {
        Texture::Var.Release(_texture);
      }
      _texture = Texture::Var.New();

      _texture->width = input.payload.imageValue.width;
      _texture->height = input.payload.imageValue.height;
      _texture->channels = input.payload.imageValue.channels;
      _texture->bpp = bpp;

      BGFX_TEXTURE2D_CREATE(bpp * 8, _texture->channels, _texture);

      if (_texture->handle.idx == bgfx::kInvalidHandle)
        throw ActivationError("Failed to create texture");
    }

    // we copy because bgfx is multithreaded
    // this just queues this texture basically
    // this copy is internally managed
    auto mem =
        bgfx::copy(input.payload.imageValue.data,
                   uint32_t(_texture->width) * uint32_t(_texture->height) *
                       uint32_t(_texture->channels) * uint32_t(_texture->bpp));

    bgfx::updateTexture2D(_texture->handle, 0, 0, 0, 0, _texture->width,
                          _texture->height, mem);

    return Texture::Var.Get(_texture);
  }
};

template <char SHADER_TYPE> struct Shader : public BaseConsumer {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return ShaderHandle::ObjType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);
    return ShaderHandle::ObjType;
  }

  static inline Parameters f_v_params{
      {"VertexShader",
       CBCCSTR("The vertex shader bytecode."),
       {CoreInfo::BytesType, CoreInfo::BytesVarType}},
      {"PixelShader",
       CBCCSTR("The pixel shader bytecode."),
       {CoreInfo::BytesType, CoreInfo::BytesVarType}}};

  static inline Parameters c_params{
      {"ComputeShader",
       CBCCSTR("The compute shader bytecode."),
       {CoreInfo::BytesType, CoreInfo::BytesVarType}}};

  static CBParametersInfo parameters() {
    if constexpr (SHADER_TYPE == 'c') {
      return c_params;
    } else {
      return f_v_params;
    }
  }

  ParamVar _vcode{};
  ParamVar _pcode{};
  ParamVar _ccode{};
  ShaderHandle *_output{nullptr};
  std::array<CBExposedTypeInfo, 3> _requiring;

  CBExposedTypesInfo requiredVariables() {
    int idx = 0;
    _requiring[idx] = BaseConsumer::ContextInfo;
    idx++;

    if constexpr (SHADER_TYPE == 'c') {
      if (_ccode.isVariable()) {
        _requiring[idx].name = _ccode.variableName();
        _requiring[idx].help = CBCCSTR("The required compute shader bytecode.");
        _requiring[idx].exposedType = CoreInfo::BytesType;
        idx++;
      }
    } else {
      if (_vcode.isVariable()) {
        _requiring[idx].name = _vcode.variableName();
        _requiring[idx].help = CBCCSTR("The required vertex shader bytecode.");
        _requiring[idx].exposedType = CoreInfo::BytesType;
        idx++;
      }
      if (_pcode.isVariable()) {
        _requiring[idx].name = _pcode.variableName();
        _requiring[idx].help = CBCCSTR("The required pixel shader bytecode.");
        _requiring[idx].exposedType = CoreInfo::BytesType;
        idx++;
      }
    }
    return {_requiring.data(), uint32_t(idx), 0};
  }

  void setParam(int index, const CBVar &value) {
    if constexpr (SHADER_TYPE == 'c') {
      switch (index) {
      case 0:
        _ccode = value;
        break;
      default:
        break;
      }
    } else {
      switch (index) {
      case 0:
        _vcode = value;
        break;
      case 1:
        _pcode = value;
        break;
      default:
        break;
      }
    }
  }

  CBVar getParam(int index) {
    if constexpr (SHADER_TYPE == 'c') {
      switch (index) {
      case 0:
        return _ccode;
      default:
        return Var::Empty;
      }
    } else {
      switch (index) {
      case 0:
        return _vcode;
      case 1:
        return _pcode;
      default:
        return Var::Empty;
      }
    }
  }

  void cleanup() {
    if constexpr (SHADER_TYPE == 'c') {
      _ccode.cleanup();
    } else {
      _vcode.cleanup();
      _pcode.cleanup();
    }

    if (_output) {
      ShaderHandle::Var.Release(_output);
      _output = nullptr;
    }
  }

  void warmup(CBContext *context) {
    if constexpr (SHADER_TYPE == 'c') {
      _ccode.warmup(context);
    } else {
      _vcode.warmup(context);
      _pcode.warmup(context);
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if constexpr (SHADER_TYPE == 'c') {
      const auto &code = _ccode.get();
      auto mem = bgfx::copy(code.payload.bytesValue, code.payload.bytesSize);
      auto sh = bgfx::createShader(mem);
      if (sh.idx == bgfx::kInvalidHandle) {
        throw ActivationError("Failed to load compute shader.");
      }
      auto ph = bgfx::createProgram(sh, true);
      if (ph.idx == bgfx::kInvalidHandle) {
        throw ActivationError("Failed to create compute shader program.");
      }

      if (_output) {
        ShaderHandle::Var.Release(_output);
      }
      _output = ShaderHandle::Var.New();
      _output->handle = ph;
    } else {
      const auto &vcode = _vcode.get();
      auto vmem = bgfx::copy(vcode.payload.bytesValue, vcode.payload.bytesSize);
      auto vsh = bgfx::createShader(vmem);
      if (vsh.idx == bgfx::kInvalidHandle) {
        throw ActivationError("Failed to load vertex shader.");
      }

      const auto &pcode = _pcode.get();
      auto pmem = bgfx::copy(pcode.payload.bytesValue, pcode.payload.bytesSize);
      auto psh = bgfx::createShader(pmem);
      if (psh.idx == bgfx::kInvalidHandle) {
        throw ActivationError("Failed to load pixel shader.");
      }

      auto ph = bgfx::createProgram(vsh, psh, true);
      if (ph.idx == bgfx::kInvalidHandle) {
        throw ActivationError("Failed to create shader program.");
      }

      if (_output) {
        ShaderHandle::Var.Release(_output);
      }
      _output = ShaderHandle::Var.New();
      _output->handle = ph;
    }
    return ShaderHandle::Var.Get(_output);
  }
};

struct Model : public BaseConsumer {
  enum class VertexAttribute {
    Position,  //!< a_position
    Normal,    //!< a_normal
    Tangent3,  //!< a_tangent
    Tangent4,  //!< a_tangent with handedness
    Bitangent, //!< a_bitangent
    Color0,    //!< a_color0
    Color1,    //!< a_color1
    Color2,    //!< a_color2
    Color3,    //!< a_color3
    Indices,   //!< a_indices
    Weight,    //!< a_weight
    TexCoord0, //!< a_texcoord0
    TexCoord1, //!< a_texcoord1
    TexCoord2, //!< a_texcoord2
    TexCoord3, //!< a_texcoord3
    TexCoord4, //!< a_texcoord4
    TexCoord5, //!< a_texcoord5
    TexCoord6, //!< a_texcoord6
    TexCoord7, //!< a_texcoord7
    Skip       // skips a byte
  };

  static bgfx::Attrib::Enum toBgfx(VertexAttribute attribute) {
    switch (attribute) {
    case VertexAttribute::Position:
      return bgfx::Attrib::Position;
    case VertexAttribute::Normal:
      return bgfx::Attrib::Normal;
    case VertexAttribute::Tangent3:
    case VertexAttribute::Tangent4:
      return bgfx::Attrib::Tangent;
    case VertexAttribute::Bitangent:
      return bgfx::Attrib::Bitangent;
    case VertexAttribute::Color0:
      return bgfx::Attrib::Color0;
    case VertexAttribute::Color1:
      return bgfx::Attrib::Color1;
    case VertexAttribute::Color2:
      return bgfx::Attrib::Color2;
    case VertexAttribute::Color3:
      return bgfx::Attrib::Color3;
    case VertexAttribute::Indices:
      return bgfx::Attrib::Indices;
    case VertexAttribute::Weight:
      return bgfx::Attrib::Weight;
    case VertexAttribute::TexCoord0:
      return bgfx::Attrib::TexCoord0;
    case VertexAttribute::TexCoord1:
      return bgfx::Attrib::TexCoord1;
    case VertexAttribute::TexCoord2:
      return bgfx::Attrib::TexCoord2;
    case VertexAttribute::TexCoord3:
      return bgfx::Attrib::TexCoord3;
    case VertexAttribute::TexCoord4:
      return bgfx::Attrib::TexCoord4;
    case VertexAttribute::TexCoord5:
      return bgfx::Attrib::TexCoord5;
    case VertexAttribute::TexCoord6:
      return bgfx::Attrib::TexCoord6;
    case VertexAttribute::TexCoord7:
      return bgfx::Attrib::TexCoord7;
    default:
      throw CBException("Invalid toBgfx case");
    }
  }

  typedef EnumInfo<VertexAttribute> VertexAttributeInfo;
  static inline VertexAttributeInfo sVertexAttributeInfo{"VertexAttribute",
                                                         CoreCC, 'gfxV'};
  static inline Type VertexAttributeType = Type::Enum(CoreCC, 'gfxV');
  static inline Type VertexAttributeSeqType = Type::SeqOf(VertexAttributeType);

  static inline Types VerticesSeqTypes{
      {CoreInfo::FloatType, CoreInfo::Float2Type, CoreInfo::Float3Type,
       CoreInfo::ColorType, CoreInfo::IntType}};
  static inline Type VerticesSeq = Type::SeqOf(VerticesSeqTypes);
  // TODO support other topologies then triangle list
  static inline Types IndicesSeqTypes{{CoreInfo::Int3Type}};
  static inline Type IndicesSeq = Type::SeqOf(IndicesSeqTypes);
  static inline Types InputTableTypes{{VerticesSeq, IndicesSeq}};
  static inline std::array<CBString, 2> InputTableKeys{"Vertices", "Indices"};
  static inline Type InputTable =
      Type::TableOf(InputTableTypes, InputTableKeys);

  static CBTypesInfo inputTypes() { return InputTable; }
  static CBTypesInfo outputTypes() { return ModelHandle::ObjType; }

  static inline Parameters params{
      {"Layout",
       CBCCSTR("The vertex layout of this model."),
       {VertexAttributeSeqType}},
      {"Dynamic",
       CBCCSTR("If the model is dynamic and will be optimized to change as "
               "often as every frame."),
       {CoreInfo::BoolType}},
      {"CullMode",
       CBCCSTR("Triangles facing the specified direction are not drawn."),
       {Enums::CullModeType}}};

  static CBParametersInfo parameters() { return params; }

  std::vector<Var> _layout;
  bgfx::VertexLayout _blayout;
  std::vector<CBType> _expectedTypes;
  size_t _lineElems{0};
  size_t _elemSize{0};
  bool _dynamic{false};
  ModelHandle *_output{nullptr};
  Enums::CullMode _cullMode{Enums::CullMode::Back};

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _layout = std::vector<Var>(Var(value));
      break;
    case 1:
      _dynamic = value.payload.boolValue;
      break;
    case 2:
      _cullMode = Enums::CullMode(value.payload.enumValue);
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_layout);
    case 1:
      return Var(_dynamic);
    case 2:
      return Var::Enum(_cullMode, CoreCC, Enums::CullModeCC);
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_output) {
      ModelHandle::Var.Release(_output);
      _output = nullptr;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);

    if (_dynamic) {
      OVERRIDE_ACTIVATE(data, activateDynamic);
    } else {
      OVERRIDE_ACTIVATE(data, activateStatic);
    }

    _expectedTypes.clear();
    _elemSize = 0;
    bgfx::VertexLayout layout;
    layout.begin();
    for (auto &entry : _layout) {
      auto e = VertexAttribute(entry.payload.enumValue);
      auto elems = 0;
      auto atype = bgfx::AttribType::Float;
      auto normalized = false;
      switch (e) {
      case VertexAttribute::Skip:
        layout.skip(1);
        _expectedTypes.emplace_back(CBType::None);
        _elemSize += 1;
        continue;
      case VertexAttribute::Position:
      case VertexAttribute::Normal:
      case VertexAttribute::Tangent3:
      case VertexAttribute::Bitangent: {
        elems = 3;
        atype = bgfx::AttribType::Float;
        _expectedTypes.emplace_back(CBType::Float3);
        _elemSize += 12;
      } break;
      case VertexAttribute::Tangent4:
      // w includes handedness
      case VertexAttribute::Weight: {
        elems = 4;
        atype = bgfx::AttribType::Float;
        _expectedTypes.emplace_back(CBType::Float4);
        _elemSize += 16;
      } break;
      case VertexAttribute::TexCoord0:
      case VertexAttribute::TexCoord1:
      case VertexAttribute::TexCoord2:
      case VertexAttribute::TexCoord3:
      case VertexAttribute::TexCoord4:
      case VertexAttribute::TexCoord5:
      case VertexAttribute::TexCoord6:
      case VertexAttribute::TexCoord7: {
        elems = 2;
        atype = bgfx::AttribType::Float;
        _expectedTypes.emplace_back(CBType::Float2);
        _elemSize += 8;
      } break;
      case VertexAttribute::Color0:
      case VertexAttribute::Color1:
      case VertexAttribute::Color2:
      case VertexAttribute::Color3: {
        elems = 4;
        normalized = true;
        atype = bgfx::AttribType::Uint8;
        _expectedTypes.emplace_back(CBType::Color);
        _elemSize += 4;
      } break;
      case VertexAttribute::Indices: {
        elems = 4;
        atype = bgfx::AttribType::Uint8;
        _expectedTypes.emplace_back(CBType::Int4);
        _elemSize += 4;
      } break;
      default:
        throw ComposeError("Invalid VertexAttribute");
      }
      layout.add(toBgfx(e), elems, atype, normalized);
    }
    layout.end();

    _blayout = layout;
    _lineElems = _expectedTypes.size();

    return ModelHandle::ObjType;
  }

  CBVar activateStatic(CBContext *context, const CBVar &input) {
    // this is the most efficient way to find items in table
    // without hashing and possible allocations etc
    CBTable table = input.payload.tableValue;
    CBTableIterator it;
    table.api->tableGetIterator(table, &it);
    CBVar vertices{};
    CBVar indices{};
    while (true) {
      CBString k;
      CBVar v;
      if (!table.api->tableNext(table, &it, &k, &v))
        break;

      switch (k[0]) {
      case 'V':
        vertices = v;
        break;
      case 'I':
        indices = v;
        break;
      default:
        break;
      }
    }

    assert(vertices.valueType == Seq && indices.valueType == Seq);

    if (!_output) {
      _output = ModelHandle::Var.New();
    } else {
      // in the case of static model, we destroy it
      // well, release, some variable might still be using it
      ModelHandle::Var.Release(_output);
      _output = ModelHandle::Var.New();
    }

    ModelHandle::StaticModel model;

    const auto nElems = size_t(vertices.payload.seqValue.len);
    if ((nElems % _lineElems) != 0) {
      throw ActivationError("Invalid amount of vertex buffer elements");
    }
    const auto line = nElems / _lineElems;
    const auto size = line * _elemSize;

    auto buffer = bgfx::alloc(size);
    size_t offset = 0;

    for (size_t i = 0; i < nElems; i += _lineElems) {
      size_t idx = 0;
      for (auto expected : _expectedTypes) {
        const auto &elem = vertices.payload.seqValue.elements[i + idx];
        // allow colors to be also Int
        if (expected == CBType::Color && elem.valueType == CBType::Int) {
          expected = CBType::Int;
        }

        if (elem.valueType != expected) {
          CBLOG_ERROR("Expected vertex element of type: {} got instead: {}",
                      type2Name(expected), type2Name(elem.valueType));
          throw ActivationError("Invalid vertex buffer element type");
        }

        switch (elem.valueType) {
        case CBType::None:
          offset += 1;
          break;
        case CBType::Float2:
          memcpy(buffer->data + offset, &elem.payload.float2Value,
                 sizeof(float) * 2);
          offset += sizeof(float) * 2;
          break;
        case CBType::Float3:
          memcpy(buffer->data + offset, &elem.payload.float3Value,
                 sizeof(float) * 3);
          offset += sizeof(float) * 3;
          break;
        case CBType::Float4:
          memcpy(buffer->data + offset, &elem.payload.float4Value,
                 sizeof(float) * 4);
          offset += sizeof(float) * 4;
          break;
        case CBType::Int4: {
          if (elem.payload.int4Value[0] < 0 ||
              elem.payload.int4Value[0] > 255 ||
              elem.payload.int4Value[1] < 0 ||
              elem.payload.int4Value[1] > 255 ||
              elem.payload.int4Value[2] < 0 ||
              elem.payload.int4Value[2] > 255 ||
              elem.payload.int4Value[3] < 0 ||
              elem.payload.int4Value[3] > 255) {
            throw ActivationError(
                "Int4 value must be between 0 and 255 for a vertex buffer");
          }
          CBColor value;
          value.r = uint8_t(elem.payload.int4Value[0]);
          value.g = uint8_t(elem.payload.int4Value[1]);
          value.b = uint8_t(elem.payload.int4Value[2]);
          value.a = uint8_t(elem.payload.int4Value[3]);
          memcpy(buffer->data + offset, &value.r, 4);
          offset += 4;
        } break;
        case CBType::Color:
          memcpy(buffer->data + offset, &elem.payload.colorValue.r, 4);
          offset += 4;
          break;
        case CBType::Int: {
          uint32_t intColor = uint32_t(elem.payload.intValue);
          memcpy(buffer->data + offset, &intColor, 4);
          offset += 4;
        } break;
        default:
          throw ActivationError("Invalid type for a vertex buffer");
        }
        idx++;
      }
    }

    const auto nindices = size_t(indices.payload.seqValue.len);
    uint16_t flags = BGFX_BUFFER_NONE;
    size_t isize = 0;
    bool compressed = true;
    if (nindices > UINT16_MAX) {
      flags |= BGFX_BUFFER_INDEX32;
      isize = nindices * sizeof(uint32_t) * 3; // int3s
      compressed = false;
    } else {
      isize = nindices * sizeof(uint16_t) * 3; // int3s
    }

    auto ibuffer = bgfx::alloc(isize);
    offset = 0;

    auto &selems = indices.payload.seqValue.elements;
    for (size_t i = 0; i < nindices; i++) {
      const static Var min{0, 0, 0};
      const Var max{int(nindices * 3), int(nindices * 3), int(nindices * 3)};
      if (compressed) {
        if (selems[i] < min || selems[i] > max) {
          throw ActivationError("Vertex index out of range");
        }
        const uint16_t t[] = {uint16_t(selems[i].payload.int3Value[0]),
                              uint16_t(selems[i].payload.int3Value[1]),
                              uint16_t(selems[i].payload.int3Value[2])};
        memcpy(ibuffer->data + offset, t, sizeof(uint16_t) * 3);
        offset += sizeof(uint16_t) * 3;
      } else {
        if (selems[i] < min || selems[i] > max) {
          throw ActivationError("Vertex index out of range");
        }
        memcpy(ibuffer->data + offset, &selems[i].payload.int3Value,
               sizeof(uint32_t) * 3);
        offset += sizeof(uint32_t) * 3;
      }
    }

    model.vb = bgfx::createVertexBuffer(buffer, _blayout);
    model.ib = bgfx::createIndexBuffer(ibuffer, flags);
    _output->cullMode = _cullMode;

    _output->model = model;

    return ModelHandle::Var.Get(_output);
  }

  CBVar activateDynamic(CBContext *context, const CBVar &input) {
    return ModelHandle::Var.Get(_output);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    throw ActivationError("Invalid activation function");
  }
};

struct CameraBase : public BaseConsumer {
  // TODO must expose view, proj and viewproj in a stack fashion probably
  static inline Types InputTableTypes{
      {CoreInfo::Float3Type, CoreInfo::Float3Type}};
  static inline std::array<CBString, 2> InputTableKeys{"Position", "Target"};
  static inline Type InputTable =
      Type::TableOf(InputTableTypes, InputTableKeys);
  static inline Types InputTypes{{CoreInfo::NoneType, InputTable}};

  static CBTypesInfo inputTypes() { return InputTypes; }
  static CBTypesInfo outputTypes() { return InputTypes; }

  int _width = 0;
  int _height = 0;
  int _offsetX = 0;
  int _offsetY = 0;
  CBVar *_bgfxContext{nullptr};

  static inline Parameters params{
      {"OffsetX",
       CBCCSTR("The horizontal offset of the viewport."),
       {CoreInfo::IntType}},
      {"OffsetY",
       CBCCSTR("The vertical offset of the viewport."),
       {CoreInfo::IntType}},
      {"Width",
       CBCCSTR("The width of the viewport, if 0 it will use the full current "
               "view width."),
       {CoreInfo::IntType}},
      {"Height",
       CBCCSTR("The height of the viewport, if 0 it will use the full current "
               "view height."),
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return params; }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);
    return data.inputType;
  }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _offsetX = int(value.payload.intValue);
      break;
    case 1:
      _offsetY = int(value.payload.intValue);
      break;
    case 2:
      _width = int(value.payload.intValue);
      break;
    case 3:
      _height = int(value.payload.intValue);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_offsetX);
    case 1:
      return Var(_offsetY);
    case 2:
      return Var(_width);
    case 3:
      return Var(_height);
    default:
      throw InvalidParameterIndex();
    }
  }

  void warmup(CBContext *context) {
    _bgfxContext = referenceVariable(context, "GFX.Context");
  }

  void cleanup() {
    if (_bgfxContext) {
      releaseVariable(_bgfxContext);
      _bgfxContext = nullptr;
    }
  }
};

struct Camera : public CameraBase {
  float _near = 0.1;
  float _far = 1000.0;
  float _fov = 60.0;

  static inline Parameters params{
      {{"Near",
        CBCCSTR("The distance from the near clipping plane."),
        {CoreInfo::FloatType}},
       {"Far",
        CBCCSTR("The distance from the far clipping plane."),
        {CoreInfo::FloatType}},
       {"FieldOfView",
        CBCCSTR("The field of view of the camera."),
        {CoreInfo::FloatType}}},
      CameraBase::params};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _near = float(value.payload.floatValue);
      break;
    case 1:
      _far = float(value.payload.floatValue);
      break;
    case 2:
      _fov = float(value.payload.floatValue);
      break;
    default:
      CameraBase::setParam(index - 3, value);
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_near);
    case 1:
      return Var(_far);
    case 2:
      return Var(_fov);
    default:
      return CameraBase::getParam(index - 3);
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    Context *ctx =
        reinterpret_cast<Context *>(_bgfxContext->payload.objectValue);

    auto &currentView = ctx->currentView();

    std::array<float, 16> view;
    if (input.valueType == CBType::Table) {
      // this is the most efficient way to find items in table
      // without hashing and possible allocations etc
      CBTable table = input.payload.tableValue;
      CBTableIterator it;
      table.api->tableGetIterator(table, &it);
      CBVar position{};
      CBVar target{};
      while (true) {
        CBString k;
        CBVar v;
        if (!table.api->tableNext(table, &it, &k, &v))
          break;

        switch (k[0]) {
        case 'P':
          position = v;
          break;
        case 'T':
          target = v;
          break;
        default:
          break;
        }
      }

      assert(position.valueType == CBType::Float3 &&
             target.valueType == CBType::Float3);

      bx::Vec3 *bp =
          reinterpret_cast<bx::Vec3 *>(&position.payload.float3Value);
      bx::Vec3 *bt = reinterpret_cast<bx::Vec3 *>(&target.payload.float3Value);
      bx::mtxLookAt(view.data(), *bp, *bt, {0.0, 1.0, 0.0},
                    bx::Handness::Right);
    }

    int width = _width != 0 ? _width : currentView.width;
    int height = _height != 0 ? _height : currentView.height;

    std::array<float, 16> proj;
    bx::mtxProj(proj.data(), _fov, float(width) / float(height), _near, _far,
                bgfx::getCaps()->homogeneousDepth, bx::Handness::Right);

    if constexpr (CurrentRenderer == Renderer::OpenGL) {
      // workaround for flipped Y render to textures
      if (currentView.id > 0) {
        proj[5] = -proj[5];
        proj[8] = -proj[8];
        proj[9] = -proj[9];
      }
    }

    bgfx::setViewTransform(
        currentView.id,
        input.valueType == CBType::Table ? view.data() : nullptr, proj.data());
    bgfx::setViewRect(currentView.id, uint16_t(_offsetX), uint16_t(_offsetY),
                      uint16_t(width), uint16_t(height));

    // set viewport params
    currentView.viewport.x = _offsetX;
    currentView.viewport.y = _offsetY;
    currentView.viewport.width = width;
    currentView.viewport.height = height;

    // populate view matrices
    if (input.valueType != CBType::Table) {
      currentView.view = linalg::identity;
    } else {
      currentView.view = Mat4::FromArray(view);
    }
    currentView.proj = Mat4::FromArray(proj);
    currentView.invalidate();

    return input;
  }
};

struct CameraOrtho : public CameraBase {
  float _near = 0.0;
  float _far = 100.0;
  float _left = 0.0;
  float _right = 1.0;
  float _bottom = 1.0;
  float _top = 0.0;

  static inline Parameters params{
      {{"Left", CBCCSTR("The left of the projection."), {CoreInfo::FloatType}},
       {"Right",
        CBCCSTR("The right of the projection."),
        {CoreInfo::FloatType}},
       {"Bottom",
        CBCCSTR("The bottom of the projection."),
        {CoreInfo::FloatType}},
       {"Top", CBCCSTR("The top of the projection."), {CoreInfo::FloatType}},
       {"Near",
        CBCCSTR("The distance from the near clipping plane."),
        {CoreInfo::FloatType}},
       {"Far",
        CBCCSTR("The distance from the far clipping plane."),
        {CoreInfo::FloatType}}},
      CameraBase::params};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _left = float(value.payload.floatValue);
      break;
    case 1:
      _right = float(value.payload.floatValue);
      break;
    case 2:
      _bottom = float(value.payload.floatValue);
      break;
    case 3:
      _top = float(value.payload.floatValue);
      break;
    case 4:
      _near = float(value.payload.floatValue);
      break;
    case 5:
      _far = float(value.payload.floatValue);
      break;
    default:
      CameraBase::setParam(index - 6, value);
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_left);
    case 1:
      return Var(_right);
    case 2:
      return Var(_bottom);
    case 3:
      return Var(_top);
    case 4:
      return Var(_near);
    case 5:
      return Var(_far);
    default:
      return CameraBase::getParam(index - 6);
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    Context *ctx =
        reinterpret_cast<Context *>(_bgfxContext->payload.objectValue);

    auto &currentView = ctx->currentView();

    std::array<float, 16> view;
    if (input.valueType == CBType::Table) {
      // this is the most efficient way to find items in table
      // without hashing and possible allocations etc
      CBTable table = input.payload.tableValue;
      CBTableIterator it;
      table.api->tableGetIterator(table, &it);
      CBVar position{};
      CBVar target{};
      while (true) {
        CBString k;
        CBVar v;
        if (!table.api->tableNext(table, &it, &k, &v))
          break;

        switch (k[0]) {
        case 'P':
          position = v;
          break;
        case 'T':
          target = v;
          break;
        default:
          break;
        }
      }

      assert(position.valueType == CBType::Float3 &&
             target.valueType == CBType::Float3);

      bx::Vec3 *bp =
          reinterpret_cast<bx::Vec3 *>(&position.payload.float3Value);
      bx::Vec3 *bt = reinterpret_cast<bx::Vec3 *>(&target.payload.float3Value);
      bx::mtxLookAt(view.data(), *bp, *bt);
    }

    int width = _width != 0 ? _width : currentView.width;
    int height = _height != 0 ? _height : currentView.height;

    std::array<float, 16> proj;
    bx::mtxOrtho(proj.data(), _left, _right, _bottom, _top, _near, _far, 0.0,
                 bgfx::getCaps()->homogeneousDepth, bx::Handness::Right);

    if constexpr (CurrentRenderer == Renderer::OpenGL) {
      // workaround for flipped Y render to textures
      if (currentView.id > 0) {
        proj[5] = -proj[5];
        proj[8] = -proj[8];
        proj[9] = -proj[9];
      }
    }

    bgfx::setViewTransform(
        currentView.id,
        input.valueType == CBType::Table ? view.data() : nullptr, proj.data());
    bgfx::setViewRect(currentView.id, uint16_t(_offsetX), uint16_t(_offsetY),
                      uint16_t(width), uint16_t(height));

    // set viewport params
    currentView.viewport.x = _offsetX;
    currentView.viewport.y = _offsetY;
    currentView.viewport.width = width;
    currentView.viewport.height = height;

    // populate view matrices
    if (input.valueType != CBType::Table) {
      currentView.view = linalg::identity;
    } else {
      currentView.view = Mat4::FromArray(view);
    }
    currentView.proj = Mat4::FromArray(proj);
    currentView.invalidate();

    return input;
  }
};

struct Draw : public BaseConsumer {
  // a matrix (in the form of 4 float4s)
  // or multiple matrices (will draw multiple times, instanced TODO)
  static CBTypesInfo inputTypes() { return CoreInfo::Float4x4Types; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float4x4Types; }

  // keep in mind that bgfx does its own sorting, so we don't need to make this
  // block way too smart
  static inline Parameters params{
      {"Shader",
       CBCCSTR("The shader program to use for this draw."),
       {ShaderHandle::ObjType, ShaderHandle::VarType}},
      {"Textures",
       CBCCSTR("A texture or the textures to pass to the shaders."),
       {Texture::ObjType, Texture::VarType, Texture::SeqType,
        Texture::VarSeqType, CoreInfo::NoneType}},
      {"Model",
       CBCCSTR("The model to draw. If no model is specified a full screen quad "
               "will be used."),
       {ModelHandle::ObjType, ModelHandle::VarType, CoreInfo::NoneType}},
      {"Blend",
       CBCCSTR(
           "The blend state table to describe and enable blending. If it's a "
           "single table the state will be assigned to both RGB and Alpha, if "
           "2 tables are specified, the first will be RGB, the second Alpha."),
       {CoreInfo::NoneType, Enums::BlendTable, Enums::BlendTableSeq}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _shader = value;
      break;
    case 1:
      _textures = value;
      break;
    case 2:
      _model = value;
      break;
    case 3:
      _blend = value;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _shader;
    case 1:
      return _textures;
    case 2:
      return _model;
    case 3:
      return _blend;
    default:
      return Var::Empty;
    }
  }

  ParamVar _shader{};
  ParamVar _textures{};
  ParamVar _model{};
  OwnedVar _blend{};
  CBVar *_bgfxContext{nullptr};
  std::array<CBExposedTypeInfo, 5> _requiring;

  CBExposedTypesInfo requiredVariables() {
    int idx = 0;
    _requiring[idx] = BaseConsumer::ContextInfo;
    idx++;

    if (_shader.isVariable()) {
      _requiring[idx].name = _shader.variableName();
      _requiring[idx].help = CBCCSTR("The required shader.");
      _requiring[idx].exposedType = ShaderHandle::ObjType;
      idx++;
    }
    if (_textures.isVariable()) {
      _requiring[idx].name = _textures.variableName();
      _requiring[idx].help = CBCCSTR("The required texture.");
      _requiring[idx].exposedType = Texture::ObjType;
      idx++;
      _requiring[idx].name = _textures.variableName();
      _requiring[idx].help = CBCCSTR("The required textures.");
      _requiring[idx].exposedType = Texture::SeqType;
      idx++;
    }
    if (_model.isVariable()) {
      _requiring[idx].name = _model.variableName();
      _requiring[idx].help = CBCCSTR("The required model.");
      _requiring[idx].exposedType = ModelHandle::ObjType;
      idx++;
    }
    return {_requiring.data(), uint32_t(idx), 0};
  }

  void warmup(CBContext *context) {
    _shader.warmup(context);
    _textures.warmup(context);
    _model.warmup(context);

    _bgfxContext = referenceVariable(context, "GFX.Context");
  }

  void cleanup() {
    _shader.cleanup();
    _textures.cleanup();
    _model.cleanup();

    if (_bgfxContext) {
      releaseVariable(_bgfxContext);
      _bgfxContext = nullptr;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);

    if (data.inputType.seqTypes.elements[0].basicType == CBType::Seq) {
      // TODO
      OVERRIDE_ACTIVATE(data, activate);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return data.inputType;
  }

  struct PosTexCoord0Vertex {
    float m_x;
    float m_y;
    float m_z;
    float m_u;
    float m_v;

    static void init() {
      ms_layout.begin()
          .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
          .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
          .end();
    }

    static inline bgfx::VertexLayout ms_layout;
  };

  static inline bool LayoutSetup{false};

  void setup() {
    if (!LayoutSetup) {
      PosTexCoord0Vertex::init();
      LayoutSetup = true;
    }
  }

  void screenSpaceQuad(float width = 1.0f, float height = 1.0f) {
    if (3 ==
        bgfx::getAvailTransientVertexBuffer(3, PosTexCoord0Vertex::ms_layout)) {
      bgfx::TransientVertexBuffer vb;
      bgfx::allocTransientVertexBuffer(&vb, 3, PosTexCoord0Vertex::ms_layout);
      PosTexCoord0Vertex *vertex = (PosTexCoord0Vertex *)vb.data;

      const float zz = 0.0f;

      const float minx = -width;
      const float maxx = width;
      const float miny = 0.0f;
      const float maxy = height * 2.0f;

      const float minu = -1.0f;
      const float maxu = 1.0f;

      float minv = 0.0f;
      float maxv = 2.0f;

      vertex[0].m_x = minx;
      vertex[0].m_y = miny;
      vertex[0].m_z = zz;
      vertex[0].m_u = minu;
      vertex[0].m_v = minv;

      vertex[1].m_x = maxx;
      vertex[1].m_y = miny;
      vertex[1].m_z = zz;
      vertex[1].m_u = maxu;
      vertex[1].m_v = minv;

      vertex[2].m_x = maxx;
      vertex[2].m_y = maxy;
      vertex[2].m_z = zz;
      vertex[2].m_u = maxu;
      vertex[2].m_v = maxv;

      bgfx::setVertexBuffer(0, &vb);
    }
  }

  void render() {
    auto *ctx = reinterpret_cast<Context *>(_bgfxContext->payload.objectValue);
    auto shader =
        reinterpret_cast<ShaderHandle *>(_shader.get().payload.objectValue);
    assert(shader);
    auto model =
        reinterpret_cast<ModelHandle *>(_model.get().payload.objectValue);

    const auto &currentView = ctx->currentView();

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;

    const auto getBlends = [&](CBVar &blend) {
      uint64_t s = 0;
      uint64_t d = 0;
      uint64_t o = 0;
      ForEach(blend.payload.tableValue, [&](auto key, auto &val) {
        if (key[0] == 'S')
          s = Enums::BlendToBgfx(val.payload.enumValue);
        else if (key[0] == 'D')
          d = Enums::BlendToBgfx(val.payload.enumValue);
        else if (key[0] == 'O')
          o = Enums::BlendOpToBgfx(val.payload.enumValue);
      });
      return std::make_tuple(s, d, o);
    };

    if (_blend.valueType == Table) {
      const auto [s, d, o] = getBlends(_blend);
      state |= BGFX_STATE_BLEND_FUNC(s, d) | BGFX_STATE_BLEND_EQUATION(o);
    } else if (_blend.valueType == Seq) {
      assert(_blend.payload.seqValue.len == 2);
      const auto [sc, dc, oc] = getBlends(_blend.payload.seqValue.elements[0]);
      const auto [sa, da, oa] = getBlends(_blend.payload.seqValue.elements[1]);
      state |= BGFX_STATE_BLEND_FUNC_SEPARATE(sc, dc, sa, da) |
               BGFX_STATE_BLEND_EQUATION_SEPARATE(oc, oa);
    }

    if (model) {
      std::visit(
          [](auto &m) {
            // Set vertex and index buffer.
            bgfx::setVertexBuffer(0, m.vb);
            bgfx::setIndexBuffer(m.ib);
          },
          model->model);
      state |= BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;

      switch (model->cullMode) {
      case Enums::CullMode::Front: {
        if constexpr (CurrentRenderer == Renderer::OpenGL) {
          // workaround for flipped Y render to textures
          if (currentView.id > 0) {
            state |= BGFX_STATE_CULL_CW;
          } else {
            state |= BGFX_STATE_CULL_CCW;
          }
        } else {
          state |= BGFX_STATE_CULL_CCW;
        }
      } break;
      case Enums::CullMode::Back: {
        if constexpr (CurrentRenderer == Renderer::OpenGL) {
          // workaround for flipped Y render to textures
          if (currentView.id > 0) {
            state |= BGFX_STATE_CULL_CCW;
          } else {
            state |= BGFX_STATE_CULL_CW;
          }
        } else {
          state |= BGFX_STATE_CULL_CW;
        }
      } break;
      default:
        break;
      }
    } else {
      screenSpaceQuad();
    }

    // set state, (it's auto reset after submit)
    bgfx::setState(state);

    auto vtextures = _textures.get();
    if (vtextures.valueType == CBType::Object) {
      auto texture = reinterpret_cast<Texture *>(vtextures.payload.objectValue);
      bgfx::setTexture(0, ctx->getSampler(0), texture->handle);
    } else if (vtextures.valueType == CBType::Seq) {
      auto textures = vtextures.payload.seqValue;
      for (uint32_t i = 0; i < textures.len; i++) {
        auto texture = reinterpret_cast<Texture *>(
            textures.elements[i].payload.objectValue);
        bgfx::setTexture(uint8_t(i), ctx->getSampler(i), texture->handle);
      }
    }

    // Submit primitive for rendering to the current view.
    bgfx::submit(currentView.id, shader->handle);
  }

  CBVar activateSingle(CBContext *context, const CBVar &input) {
    if (input.payload.seqValue.len != 4) {
      throw ActivationError("Invalid Matrix4x4 input, should Float4 x 4.");
    }

    bgfx::Transform t;
    // using allocTransform to avoid an extra copy
    auto idx = bgfx::allocTransform(&t, 1);
    memcpy(&t.data[0], &input.payload.seqValue.elements[0].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&t.data[4], &input.payload.seqValue.elements[1].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&t.data[8], &input.payload.seqValue.elements[2].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&t.data[12], &input.payload.seqValue.elements[3].payload.float4Value,
           sizeof(float) * 4);
    bgfx::setTransform(idx, 1);

    render();

    return input;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto instances = input.payload.seqValue.len;
    constexpr uint16_t stride = 64; // matrix 4x4
    if (bgfx::getAvailInstanceDataBuffer(instances, stride) != instances) {
      throw ActivationError("Instance buffer overflow");
    }

    bgfx::InstanceDataBuffer idb;
    bgfx::allocInstanceDataBuffer(&idb, instances, stride);
    uint8_t *data = idb.data;

    for (uint32_t i = 0; i < instances; i++) {
      float *mat = reinterpret_cast<float *>(data);
      CBVar &vmat = input.payload.seqValue.elements[i];

      if (vmat.payload.seqValue.len != 4) {
        throw ActivationError("Invalid Matrix4x4 input, should Float4 x 4.");
      }

      memcpy(&mat[0], &vmat.payload.seqValue.elements[0].payload.float4Value,
             sizeof(float) * 4);
      memcpy(&mat[4], &vmat.payload.seqValue.elements[1].payload.float4Value,
             sizeof(float) * 4);
      memcpy(&mat[8], &vmat.payload.seqValue.elements[2].payload.float4Value,
             sizeof(float) * 4);
      memcpy(&mat[12], &vmat.payload.seqValue.elements[3].payload.float4Value,
             sizeof(float) * 4);

      data += stride;
    }

    bgfx::setInstanceDataBuffer(&idb);

    render();

    return input;
  }
};

struct RenderTarget : public BaseConsumer {
  static inline Parameters params{
      {"Width",
       CBCCSTR("The width of the texture to render."),
       {CoreInfo::IntType}},
      {"Height",
       CBCCSTR("The height of the texture to render."),
       {CoreInfo::IntType}},
      {"Contents",
       CBCCSTR("The blocks expressing the contents to render."),
       {CoreInfo::BlocksOrNone}},
      {"GUI",
       CBCCSTR("If this render target should be able to render GUI blocks "
               "within. If false any GUI block inside will be rendered on the "
               "top level surface."),
       {CoreInfo::BoolType}},
      {"ClearColor",
       CBCCSTR("The color to use to clear the backbuffer at the beginning of a "
               "new frame."),
       {CoreInfo::ColorType}}};
  static CBParametersInfo parameters() { return params; }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  BlocksVar _blocks;
  int _width{256};
  int _height{256};
  bool _gui{false};
  bgfx::FrameBufferHandle _framebuffer = BGFX_INVALID_HANDLE;
  CBVar *_bgfxContext{nullptr};
  bgfx::ViewId _viewId;
  std::optional<OcornutImguiContext> _imguiBgfxCtx;
  CBColor _clearColor{0x30, 0x30, 0x30, 0xFF};

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _width = int(value.payload.intValue);
      break;
    case 1:
      _height = int(value.payload.intValue);
      break;
    case 2:
      _blocks = value;
      break;
    case 3:
      _gui = value.payload.boolValue;
      break;
    case 4:
      _clearColor = value.payload.colorValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_width);
    case 1:
      return Var(_height);
    case 2:
      return _blocks;
    case 3:
      return Var(_gui);
    case 4:
      return Var(_clearColor);
    default:
      throw InvalidParameterIndex();
    }
  }
};

struct RenderTexture : public RenderTarget {
  // to make it simple our render textures are always 16bit linear
  // TODO we share same size/formats depth buffers, expose only color part

  static CBTypesInfo outputTypes() { return Texture::ObjType; }

  Texture *_texture{nullptr}; // the color part we expose
  Texture _depth{};

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);
    _blocks.compose(data);
    return Texture::ObjType;
  }

  void warmup(CBContext *context) {
    _texture = Texture::Var.New();
    _texture->handle =
        bgfx::createTexture2D(_width, _height, false, 1,
                              bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT);
    _texture->width = _width;
    _texture->height = _height;
    _texture->channels = 4;
    _texture->bpp = 2;

    _depth.handle = bgfx::createTexture2D(
        _width, _height, false, 1, bgfx::TextureFormat::D24S8,
        BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY);

    bgfx::TextureHandle textures[] = {_texture->handle, _depth.handle};
    _framebuffer = bgfx::createFrameBuffer(2, textures, false);

    _bgfxContext = referenceVariable(context, "GFX.Context");
    assert(_bgfxContext->valueType == CBType::Object);
    Context *ctx =
        reinterpret_cast<Context *>(_bgfxContext->payload.objectValue);
    _viewId = ctx->nextViewId();

    bgfx::setViewRect(_viewId, 0, 0, _width, _height);
    bgfx::setViewClear(_viewId,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                       Var(_clearColor).colorToInt(), 1.0f, 0);

    _blocks.warmup(context);
  }

  void cleanup() {
    _blocks.cleanup();

    if (_bgfxContext) {
      releaseVariable(_bgfxContext);
      _bgfxContext = nullptr;
    }

    if (_framebuffer.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(_framebuffer);
      _framebuffer.idx = bgfx::kInvalidHandle;
    }

    if (_depth.handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(_depth.handle);
      _depth.handle.idx = bgfx::kInvalidHandle;
    }

    if (_texture) {
      Texture::Var.Release(_texture);
      _texture = nullptr;
    }

    if (_imguiBgfxCtx) {
      _imguiBgfxCtx->destroy();
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    Context *ctx =
        reinterpret_cast<Context *>(_bgfxContext->payload.objectValue);

    // we could do this on warmup but breaks on window resize...
    // so let's just do it every activation, doubt its a bottleneck
    bgfx::setViewFrameBuffer(_viewId, _framebuffer);

    // push _viewId
    ctx->pushView({_viewId, _width, _height, _framebuffer});
    DEFER({ ctx->popView(); });

    // Touch _viewId
    bgfx::touch(_viewId);

    // activate the blocks and render
    CBVar output{};
    _blocks.activate(context, input, output);

    return Texture::Var.Get(_texture);
  }
};

struct SetUniform : public BaseConsumer {
  static inline Types InputTypes{
      {CoreInfo::Float4Type, CoreInfo::Float4x4Type, CoreInfo::Float3x3Type,
       CoreInfo::Float4SeqType, CoreInfo::Float4x4SeqType,
       CoreInfo::Float3x3SeqType}};

  static CBTypesInfo inputTypes() { return InputTypes; }
  static CBTypesInfo outputTypes() { return InputTypes; }

  std::string _name;
  bgfx::UniformType::Enum _type;
  bool _isArray{false};
  int _elems{1};
  CBTypeInfo _expectedType;
  bgfx::UniformHandle _handle = BGFX_INVALID_HANDLE;
  std::vector<float> _scratch;

  static inline Parameters Params{
      {"Name",
       CBCCSTR("The name of the uniform shader variable. Uniforms are so named "
               "because they do not change from one shader invocation to the "
               "next within a particular rendering call thus their value is "
               "uniform among all invocations. Uniform names are unique."),
       {CoreInfo::StringType}},
      {"MaxSize",
       CBCCSTR("If the input contains multiple values, the maximum expected "
               "size of the input."),
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return Params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _name = value.payload.stringValue;
      break;
    case 1:
      _elems = value.payload.intValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_name);
    case 1:
      return Var(_elems);
    default:
      throw InvalidParameterIndex();
    }
  }

  bgfx::UniformType::Enum findSingleType(const CBTypeInfo &t) {
    if (t.basicType == CBType::Float4) {
      return bgfx::UniformType::Vec4;
    } else if (t.basicType == CBType::Seq) {
      if (t.fixedSize == 4 &&
          t.seqTypes.elements[0].basicType == CBType::Float4) {
        return bgfx::UniformType::Mat4;
      } else if (t.fixedSize == 3 &&
                 t.seqTypes.elements[0].basicType == CBType::Float3) {
        return bgfx::UniformType::Mat3;
      }
    }
    throw ComposeError("Invalid input type for GFX.SetUniform");
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _expectedType = data.inputType;
    if (_elems == 1) {
      _type = findSingleType(data.inputType);
      _isArray = false;
    } else {
      if (data.inputType.basicType != CBType::Seq ||
          data.inputType.seqTypes.len == 0)
        throw ComposeError("Invalid input type for GFX.SetUniform");
      _type = findSingleType(data.inputType.seqTypes.elements[0]);
      _isArray = true;
    }
    return data.inputType;
  }

  void warmup(CBContext *context) {
    _handle = bgfx::createUniform(_name.c_str(), _type, !_isArray ? 1 : _elems);
  }

  void cleanup() {
    if (_handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(_handle);
      _handle.idx = bgfx::kInvalidHandle;
    }
  }

  void fillElement(const CBVar &elem, int &offset) {
    if (elem.valueType == CBType::Float4) {
      memcpy(_scratch.data() + offset, &elem.payload.float4Value,
             sizeof(float) * 4);
      offset += 4;
    } else {
      // Seq
      if (_type == bgfx::UniformType::Mat3) {
        memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[0],
               sizeof(float) * 3);
        offset += 3;
        memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[1],
               sizeof(float) * 3);
        offset += 3;
        memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[2],
               sizeof(float) * 3);
        offset += 3;
      } else {
        memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[0],
               sizeof(float) * 4);
        offset += 4;
        memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[1],
               sizeof(float) * 4);
        offset += 4;
        memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[2],
               sizeof(float) * 4);
        offset += 4;
        memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[3],
               sizeof(float) * 4);
        offset += 4;
      }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    _scratch.clear();
    uint16_t elems = 0;
    if (unlikely(_isArray)) {
      const int len = int(input.payload.seqValue.len);
      if (len > _elems) {
        throw ActivationError(
            "Input array size exceeded maximum uniform array size.");
      }
      int offset = 0;
      switch (_type) {
      case bgfx::UniformType::Vec4:
        _scratch.resize(4 * len);
        break;
      case bgfx::UniformType::Mat3:
        _scratch.resize(3 * 3 * len);
        break;
      case bgfx::UniformType::Mat4:
        _scratch.resize(4 * 4 * len);
        break;
      default:
        throw InvalidParameterIndex();
      }
      for (auto &elem : input) {
        fillElement(elem, offset);
        elems++;
      }
    } else {
      int offset = 0;
      switch (_type) {
      case bgfx::UniformType::Vec4:
        _scratch.resize(4);
        break;
      case bgfx::UniformType::Mat3:
        _scratch.resize(3 * 3);
        break;
      case bgfx::UniformType::Mat4:
        _scratch.resize(4 * 4);
        break;
      default:
        throw InvalidParameterIndex();
      }
      fillElement(input, offset);
      elems++;
    }

    bgfx::setUniform(_handle, _scratch.data(), elems);

    return input;
  }
};

struct Screenshot : public BaseConsumer {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static inline Parameters params{
      {"Overwrite",
       CBCCSTR("If each activation should overwrite the previous screenshot at "
               "the given path if existing. If false the path will be suffixed "
               "in order to avoid overwriting."),
       {CoreInfo::BoolType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    _overwrite = value.payload.boolValue;
  }

  CBVar getParam(int index) { return Var(_overwrite); }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);
    return CoreInfo::StringType;
  }

  void warmup(CBContext *context) {
    _bgfxContext = referenceVariable(context, "GFX.Context");
    assert(_bgfxContext->valueType == CBType::Object);
  }

  void cleanup() {
    if (_bgfxContext) {
      releaseVariable(_bgfxContext);
      _bgfxContext = nullptr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    Context *ctx =
        reinterpret_cast<Context *>(_bgfxContext->payload.objectValue);
    const auto &currentView = ctx->currentView();
    if (_overwrite) {
      bgfx::requestScreenShot(currentView.fb, input.payload.stringValue);
    } else {
      std::filesystem::path base(input.payload.stringValue);
      const auto baseName = base.stem().string();
      const auto ext = base.extension().string();
      uint32_t suffix = 0;
      std::filesystem::path path = base;
      while (std::filesystem::exists(path)) {
        const auto ssuffix = std::to_string(suffix++);
        const auto newName = baseName + ssuffix + ext;
        path = base.root_path() / newName;
      }
      const auto s = path.string();
      bgfx::requestScreenShot(currentView.fb, s.c_str());
    }
    return input;
  }

  CBVar *_bgfxContext{nullptr};
  bool _overwrite{true};
};

struct Unproject : public BaseConsumer {
  static CBTypesInfo inputTypes() { return CoreInfo::Float2Type; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float3Type; }

  CBVar *_bgfxContext{nullptr};
  float _z{0.0};

  static inline Parameters params{
      {"Z",
       CBCCSTR("The value of Z depth to use, generally 0.0 for  the near "
               "plane, 1.0 for the far plane."),
       {CoreInfo::FloatType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    _z = float(value.payload.floatValue);
  }

  CBVar getParam(int index) { return Var(_z); }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);
    return CoreInfo::Float3Type;
  }

  void warmup(CBContext *context) {
    _bgfxContext = referenceVariable(context, "GFX.Context");
    assert(_bgfxContext->valueType == CBType::Object);
  }

  void cleanup() {
    if (_bgfxContext) {
      releaseVariable(_bgfxContext);
      _bgfxContext = nullptr;
    }
  }

  CBOptionalString help() {
    return CBCCSTR(
        "This block unprojects screen coordinates into world coordinates.");
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    using namespace linalg;
    using namespace linalg::aliases;

    Context *ctx =
        reinterpret_cast<Context *>(_bgfxContext->payload.objectValue);
    const auto &currentView = ctx->currentView();

    const auto sx =
        float(input.payload.float2Value[0]) * float(currentView.width);
    const auto sy =
        float(input.payload.float2Value[1]) * float(currentView.height);
    const auto vx = float(currentView.viewport.x);
    const auto vy = float(currentView.viewport.y);
    const auto vw = float(currentView.viewport.width);
    const auto vh = float(currentView.viewport.height);
    const auto x = (((sx - vx) / vw) * 2.0f) - 1.0f;
    const auto y = 1.0f - (((sy - vy) / vh) * 2.0f);

    const auto m = mul(currentView.invView(), currentView.invProj());
    const auto v = mul(m, float4(x, y, _z, 1.0f));
    const Vec3 res = v / v.w;
    return res;
  }
};

struct CompileShader {
  enum class ShaderType { Vertex, Pixel, Compute };

  std::unique_ptr<IShaderCompiler> _compiler = makeShaderCompiler();
  ShaderType _type{ShaderType::Vertex};

  static inline Types InputTableTypes{
      {CoreInfo::StringType, CoreInfo::StringType, CoreInfo::StringSeqType}};
  static inline std::array<CBString, 3> InputTableKeys{"varyings", "code",
                                                       "defines"};
  static inline Type InputTable =
      Type::TableOf(InputTableTypes, InputTableKeys);

  static constexpr uint32_t ShaderTypeCC = 'gfxS';
  static inline EnumInfo<ShaderType> ShaderTypeEnumInfo{"ShaderType", CoreCC,
                                                        ShaderTypeCC};
  static inline Type ShaderTypeType = Type::Enum(CoreCC, ShaderTypeCC);

  CBTypesInfo inputTypes() { return InputTable; }
  CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  static inline Parameters Params{
      {"Type", CBCCSTR("The type of shader to compile."), {ShaderTypeType}}};

  static CBParametersInfo parameters() { return Params; }

  void setParam(int index, CBVar value) {
    _type = ShaderType(value.payload.enumValue);
  }

  CBVar getParam(int index) { return Var::Enum(_type, CoreCC, ShaderTypeCC); }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto &tab = input.payload.tableValue;
    auto vvaryings = tab.api->tableAt(tab, "varyings");
    auto varyings = CBSTRVIEW((*vvaryings));
    auto vcode = tab.api->tableAt(tab, "code");
    auto code = CBSTRVIEW((*vcode));
    auto vdefines = tab.api->tableAt(tab, "defines");
    std::string defines;
    if (vdefines->valueType != CBType::None) {
      for (auto &define : *vdefines) {
        auto d = CBSTRVIEW(define);
        defines += std::string(d) + ";";
      }
    }

    return _compiler->compile(varyings, code,
                              _type == ShaderType::Vertex  ? "v"
                              : _type == ShaderType::Pixel ? "f"
                                                           : "c",
                              defines, context);
  }
};

void registerBGFXBlocks() {
  REGISTER_CBLOCK("GFX.MainWindow", MainWindow);
  REGISTER_CBLOCK("GFX.Texture2D", Texture2D);
  using GraphicsShader = Shader<'g'>;
  REGISTER_CBLOCK("GFX.Shader", GraphicsShader);
  using ComputeShader = Shader<'c'>;
  REGISTER_CBLOCK("GFX.ComputeShader", ComputeShader);
  REGISTER_CBLOCK("GFX.Model", Model);
  REGISTER_CBLOCK("GFX.Camera", Camera);
  REGISTER_CBLOCK("GFX.CameraOrtho", CameraOrtho);
  REGISTER_CBLOCK("GFX.Draw", Draw);
  REGISTER_CBLOCK("GFX.RenderTexture", RenderTexture);
  REGISTER_CBLOCK("GFX.SetUniform", SetUniform);
  REGISTER_CBLOCK("GFX.Screenshot", Screenshot);
  REGISTER_CBLOCK("GFX.Unproject", Unproject);
  REGISTER_CBLOCK("GFX.CompileShader", CompileShader);
}
}; // namespace BGFX

#ifdef CB_INTERNAL_TESTS
#include "bgfx_tests.cpp"
#endif

namespace BGFX {
#include "SDL_syswm.h"

void *SDL_GetNativeWindowPtr(SDL_Window *window) {
  SDL_SysWMinfo winInfo{};
  SDL_version sdlVer{};
  SDL_VERSION(&sdlVer);
  winInfo.version = sdlVer;
  if (!SDL_GetWindowWMInfo(window, &winInfo)) {
    throw ActivationError("Failed to call SDL_GetWindowWMInfo");
  }
#if defined(_WIN32)
  return winInfo.info.win.window;
#elif defined(__linux__)
  return (void *)winInfo.info.x11.window;
#else
  return nullptr;
#endif
}

void *SDL_GetNativeDisplayPtr(SDL_Window *window) {
  SDL_SysWMinfo winInfo{};
  SDL_version sdlVer{};
  SDL_VERSION(&sdlVer);
  winInfo.version = sdlVer;
  if (!SDL_GetWindowWMInfo(window, &winInfo)) {
    throw ActivationError("Failed to call SDL_GetWindowWMInfo");
  }
#if defined(__linux__)
  return (void *)winInfo.info.x11.display;
#else
  return nullptr;
#endif
}
} // namespace BGFX