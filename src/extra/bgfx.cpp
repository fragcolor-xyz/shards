/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "./bgfx.hpp"
#include "./imgui.hpp"
#include "SDL.h"
#include <bx/debug.h>
#include <bx/math.h>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#include <webxr.h>
#endif

using namespace chainblocks;

namespace BGFX {
struct Base {
  CBVar *_bgfxCtx;
};

constexpr uint32_t windowCC = 'hwnd';

// delay this at end of file, otherwise we pull a header mess
#if defined(_WIN32)
HWND SDL_GetNativeWindowPtr(SDL_Window *window);
#elif defined(__linux__)
void *SDL_GetNativeWindowPtr(SDL_Window *window);
#endif

struct BaseConsumer : public Base {
  static inline Type windowType{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = windowCC}}}};

  static inline ExposedInfo requiredInfo = ExposedInfo(ExposedInfo::Variable(
      "GFX.Context", CBCCSTR("The BGFX Context."), Context::Info));

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(requiredInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("GFX Blocks cannot be used on a worker thread (e.g. "
                         "within an Await block)");
    }
    return data.inputType;
  }
};

// DPI awareness fix
#ifdef _WIN32
struct DpiAwareness {
  DpiAwareness() { SetProcessDPIAware(); }
};
#endif

struct BaseWindow : public Base {
#ifdef _WIN32
  static inline DpiAwareness DpiAware{};
  HWND _sysWnd = nullptr;
#elif defined(__APPLE__)
  void *_sysWnd = nullptr;
  SDL_MetalView _metalView{nullptr};
#elif defined(__linux__) || defined(__EMSCRIPTEN__)
  void *_sysWnd = nullptr;
#endif

  static inline Parameters params{
      {"Title",
       CBCCSTR("The title of the window to create."),
       {CoreInfo::StringType}},
      {"Width",
       CBCCSTR("The width of the window to create"),
       {CoreInfo::IntType}},
      {"Height",
       CBCCSTR("The height of the window to create."),
       {CoreInfo::IntType}},
      {"Contents",
       CBCCSTR("The contents of this window."),
       {CoreInfo::BlocksOrNone}}};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return params; }

  std::string _title;
  int _width = 1024;
  int _height = 768;
  SDL_Window *_window = nullptr;
  CBVar *_sdlWinVar = nullptr;
  CBVar *_imguiCtx = nullptr;
  CBVar *_nativeWnd = nullptr;
  BlocksVar _blocks;

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      break;
    case 1:
      _width = int(value.payload.intValue);
      break;
    case 2:
      _height = int(value.payload.intValue);
      break;
    case 3:
      _blocks = value;
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
      return Var(_width);
    case 2:
      return Var(_height);
    case 3:
      return _blocks;
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
      LOG(DEBUG) << "(bgfx): " << out;
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
      BX_UNUSED(_filePath, _width, _height, _pitch, _data, _size, _yflip);
      // TODO
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
  };

  Context _bgfx_context{};
  chainblocks::ImGui::Context _imgui_context{};
  int32_t _wheelScroll = 0;
  bool _bgfxInit{false};
  float _windowScalingW{1.0};
  float _windowScalingH{1.0};

  // TODO thread_local? anyway sort multiple threads
  static inline std::vector<SDL_Event> sdlEvents;

  CBTypeInfo compose(const CBInstanceData &data) {
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

    CBInstanceData dataCopy = data;
    arrayPush(dataCopy.shared,
              ExposedInfo::ProtectedVariable("GFX.CurrentWindow",
                                             CBCCSTR("The exposed SDL window."),
                                             BaseConsumer::windowType));
    arrayPush(dataCopy.shared,
              ExposedInfo::ProtectedVariable(
                  "GFX.Context", CBCCSTR("The BGFX Context."), Context::Info));
    arrayPush(dataCopy.shared, ExposedInfo::ProtectedVariable(
                                   "GUI.Context", CBCCSTR("The ImGui Context."),
                                   chainblocks::ImGui::Context::Info));
    _blocks.compose(dataCopy);

    return data.inputType;
  }

  void cleanup() {
    // cleanup before releasing the resources
    _blocks.cleanup();

    // _imgui_context.Reset();
    _bgfx_context.reset();

    if (_bgfxInit) {
      imguiDestroy();
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
        LOG(ERROR)
            << "MainWindow: Found a dangling reference to GFX.CurrentWindow.";
      }
      memset(_sdlWinVar, 0x0, sizeof(CBVar));
      _sdlWinVar = nullptr;
    }

    if (_bgfxCtx) {
      if (_bgfxCtx->refcount > 1) {
        LOG(ERROR) << "MainWindow: Found a dangling reference to GFX.Context.";
      }
      memset(_bgfxCtx, 0x0, sizeof(CBVar));
      _bgfxCtx = nullptr;
    }

    if (_imguiCtx) {
      if (_imguiCtx->refcount > 1) {
        LOG(ERROR) << "MainWindow: Found a dangling reference to GUI.Context.";
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

  void warmup(CBContext *context) {
    auto initErr = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
    if (initErr != 0) {
      LOG(ERROR) << "Failed to initialize SDL " << SDL_GetError();
      throw ActivationError("Failed to initialize SDL");
    }

    registerRunLoopCallback("fragcolor.gfx.ospump", [] {
      sdlEvents.clear();
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        sdlEvents.push_back(event);
      }
    });

    bgfx::Init initInfo{};

    // try to see if global native window is set
    _nativeWnd = referenceVariable(context, "fragcolor.gfx.nativewindow");
    if (_nativeWnd->valueType == Object &&
        _nativeWnd->payload.objectVendorId == CoreCC &&
        _nativeWnd->payload.objectTypeId == BgfxNativeWindowCC) {
      _sysWnd = decltype(_sysWnd)(_nativeWnd->payload.objectValue);
      // TODO SDL_CreateWindowFrom to enable inputs etc...
      // specially for iOS thing is that we pass context as variable, not a
      // window object we might need 2 variables in the end
    } else {
      uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __APPLE__
      flags |= SDL_WINDOW_METAL;
#endif
      // TODO: SDL_WINDOW_ALLOW_HIGHDPI
      // TODO: SDL_WINDOW_RESIZABLE
      // TODO: SDL_WINDOW_BORDERLESS
      _window =
          SDL_CreateWindow(_title.c_str(), SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, _width, _height, flags);

#ifdef __APPLE__
      // tricky cos retina..
      // first we must poke metal
      _metalView = SDL_Metal_CreateView(_window);
      // now we can find out the scaling
      int real_w, real_h;
      SDL_Metal_GetDrawableSize(_window, &real_w, &real_h);
      _windowScalingW = float(real_w) / float(_width);
      _windowScalingH = float(real_h) / float(_height);
      // fix the scaling now if needed
      if (_windowScalingW != 1.0 || _windowScalingH != 1.0) {
        SDL_Metal_DestroyView(_metalView);
        SDL_SetWindowSize(_window, int(float(_width) / _windowScalingW),
                          int(float(_height) / _windowScalingH));
        _metalView = SDL_Metal_CreateView(_window);
      }
      _sysWnd = SDL_Metal_GetLayer(_metalView);
#elif defined(_WIN32) || defined(__linux__)
      _sysWnd = SDL_GetNativeWindowPtr(_window);
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
    bgfx::setPlatformData(pdata);

    initInfo.resolution.width = _width;
    initInfo.resolution.height = _height;
    initInfo.resolution.reset = BGFX_RESET_VSYNC;
    if (!bgfx::init(initInfo)) {
      throw ActivationError("Failed to initialize BGFX");
    } else {
      _bgfxInit = true;
    }

#ifdef BGFX_CONFIG_RENDERER_OPENGL_MIN_VERSION
    LOG(INFO) << "Renderer version: "
              << bgfx::getRendererName(bgfx::RendererType::OpenGL);
#elif BGFX_CONFIG_RENDERER_OPENGLES_MIN_VERSION
    LOG(INFO) << "Renderer version: "
              << bgfx::getRendererName(bgfx::RendererType::OpenGLES);
#endif

    // _imgui_context.Reset();
    // _imgui_context.Set();

    imguiCreate();

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

    bgfx::setViewRect(0, 0, 0, _width, _height);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030FF, 1.0f,
                       0);

    _sdlWinVar = referenceVariable(context, "GFX.CurrentWindow");
    _bgfxCtx = referenceVariable(context, "GFX.Context");
    _imguiCtx = referenceVariable(context, "GUI.Context");

    // populate them here too to allow warmup operation
    _sdlWinVar->valueType = _bgfxCtx->valueType = _imguiCtx->valueType =
        CBType::Object;
    _sdlWinVar->payload.objectVendorId = _bgfxCtx->payload.objectVendorId =
        _imguiCtx->payload.objectVendorId = CoreCC;

    _sdlWinVar->payload.objectTypeId = windowCC;
    _sdlWinVar->payload.objectValue = _sysWnd;

    _bgfxCtx->payload.objectTypeId = BgfxContextCC;
    _bgfxCtx->payload.objectValue = &_bgfx_context;

    _imguiCtx->payload.objectTypeId = chainblocks::ImGui::ImGuiContextCC;
    _imguiCtx->payload.objectValue = &_imgui_context;

    auto nv = _bgfx_context.nextView();
    assert(nv == 0); // always 0 in MainWindow

    // init blocks after we initialize the system
    _blocks.warmup(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // push view 0
    _bgfx_context.push(0);
    DEFER({
      _bgfx_context.pop();
      assert(_bgfx_context.index() == 0);
    });
    // Touch view 0
    bgfx::touch(0);

    // _imgui_context.Set();

    ImGuiIO &io = ::ImGui::GetIO();

    // Draw imgui and deal with inputs
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
    for (auto &event : sdlEvents) {
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
        auto key = event.key.keysym.scancode;
        io.KeysDown[key] = event.type == SDL_KEYDOWN;
        io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
        io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
        io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
        io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
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
        }
      }
    }

    if (_windowScalingW != 1.0 || _windowScalingH != 1.0) {
      mouseX = int32_t(float(mouseX) * _windowScalingW);
      mouseY = int32_t(float(mouseY) * _windowScalingH);
    }

    imguiBeginFrame(mouseX, mouseY, imbtns, _wheelScroll, _width, _height);

    // activate the blocks and render
    CBVar output{};
    _blocks.activate(context, input, output);

    // finish up with this frame
    imguiEndFrame();
    bgfx::frame();

    return input;
  }
};

struct Texture2D : public BaseConsumer {
  static inline Parameters params{
      {"sRGB",
       CBCCSTR("If the texture should be loaded as an sRGB format (only valid "
               "for 8 bit per color textures)."),
       {CoreInfo::BoolType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    _srgb = value.payload.boolValue;
  }

  CBVar getParam(int index) { return Var(_srgb); }

  Texture *_texture{nullptr};
  bool _srgb{false};

  void cleanup() {
    if (_texture) {
      Texture::Var.Release(_texture);
      _texture = nullptr;
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }

  static CBTypesInfo outputTypes() { return Texture::TextureHandleType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);
    return Texture::TextureHandleType;
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

      if (_texture->bpp == 1) {
        switch (_texture->channels) {
        case 1:
          _texture->handle = bgfx::createTexture2D(
              _texture->width, _texture->height, false, 1,
              bgfx::TextureFormat::R8, _srgb ? BGFX_TEXTURE_SRGB : 0);
          break;
        case 2:
          _texture->handle = bgfx::createTexture2D(
              _texture->width, _texture->height, false, 1,
              bgfx::TextureFormat::RG8, _srgb ? BGFX_TEXTURE_SRGB : 0);
          break;
        case 3:
          _texture->handle = bgfx::createTexture2D(
              _texture->width, _texture->height, false, 1,
              bgfx::TextureFormat::RGB8, _srgb ? BGFX_TEXTURE_SRGB : 0);
          break;
        case 4:
          _texture->handle = bgfx::createTexture2D(
              _texture->width, _texture->height, false, 1,
              bgfx::TextureFormat::RGBA8, _srgb ? BGFX_TEXTURE_SRGB : 0);
          break;
        default:
          cbassert(false);
          break;
        }
      } else if (_texture->bpp == 2) {
        switch (_texture->channels) {
        case 1:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::R16U);
          break;
        case 2:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::RG16U);
          break;
        case 3:
          throw ActivationError("Format not supported, it seems bgfx has no "
                                "RGB16, try using RGBA16 instead (FillAlpha).");
          break;
        case 4:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::RGBA16U);
          break;
        default:
          cbassert(false);
          break;
        }
      } else if (_texture->bpp == 4) {
        switch (_texture->channels) {
        case 1:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::R32F);
          break;
        case 2:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::RG32F);
          break;
        case 3:
          throw ActivationError(
              "Format not supported, it seems bgfx has no RGB32F, try using "
              "RGBA32F instead (FillAlpha).");
          break;
        case 4:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::RGBA32F);
          break;
        default:
          cbassert(false);
          break;
        }
      }
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
  std::array<CBExposedTypeInfo, 2> _exposing;

  CBExposedTypesInfo requiredVariables() {
    if constexpr (SHADER_TYPE == 'c') {
      if (!_ccode.isVariable()) {
        return {};
      } else {
        _exposing[0].name = _ccode.variableName();
        _exposing[0].help = CBCCSTR("The required compute shader bytecode.");
        _exposing[0].exposedType = CoreInfo::BytesType;
        return {_exposing.data(), 1, 0};
      }
    } else {
      int idx = -1;
      if (_vcode.isVariable()) {
        idx++;
        _exposing[idx].name = _vcode.variableName();
        _exposing[idx].help = CBCCSTR("The required vertex shader bytecode.");
        _exposing[idx].exposedType = CoreInfo::BytesType;
      }
      if (_pcode.isVariable()) {
        idx++;
        _exposing[idx].name = _pcode.variableName();
        _exposing[idx].help = CBCCSTR("The required pixel shader bytecode.");
        _exposing[idx].exposedType = CoreInfo::BytesType;
      }
      if (idx == -1) {
        return {};
      } else {
        return {_exposing.data(), uint32_t(idx + 1), 0};
      }
    }
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
    Tangent,   //!< a_tangent
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
    case VertexAttribute::Tangent:
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
       {CoreInfo::BoolType}}};

  static CBParametersInfo parameters() { return params; }

  std::vector<Var> _layout;
  bgfx::VertexLayout _blayout;
  std::vector<CBType> _expectedTypes;
  size_t _lineElems{0};
  size_t _elemSize{0};
  bool _dynamic{false};
  ModelHandle *_output{nullptr};

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _layout = std::vector<Var>(Var(value));
      break;
    case 1:
      _dynamic = value.payload.boolValue;
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_layout);
    case 1:
      return Var(_dynamic);
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
      case VertexAttribute::Tangent:
      case VertexAttribute::Bitangent: {
        elems = 3;
        atype = bgfx::AttribType::Float;
        _expectedTypes.emplace_back(CBType::Float3);
        _elemSize += 12;
      } break;
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
          LOG(ERROR) << "Expected vertex element of type: "
                     << type2Name(expected)
                     << " got instead: " << type2Name(elem.valueType);
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

struct Camera : public BaseConsumer {
  // TODO must expose view, proj and viewproj in a stack fashion probably
  static inline Types InputTableTypes{
      {CoreInfo::Float3Type, CoreInfo::Float3Type}};
  static inline std::array<CBString, 2> InputTableKeys{"Position", "Target"};
  static inline Type InputTable =
      Type::TableOf(InputTableTypes, InputTableKeys);

  static CBTypesInfo inputTypes() { return InputTable; }
  static CBTypesInfo outputTypes() { return InputTable; }

  int _width = 1024;
  int _height = 768;
  float _near = 0.1;
  float _far = 1000.0;
  float _fov = 60.0;
  int _offsetX = 0;
  int _offsetY = 0;
  CBVar *_bgfx_context{nullptr};

  static inline Parameters params{
      {"Width", CBCCSTR("The width of the viewport."), {CoreInfo::IntType}},
      {"Height", CBCCSTR("The height of the viewport."), {CoreInfo::IntType}},
      {"Near",
       CBCCSTR("The distance from the near clipping plane."),
       {CoreInfo::FloatType}},
      {"Far",
       CBCCSTR("The distance from the far clipping plane."),
       {CoreInfo::FloatType}},
      {"FieldOfView",
       CBCCSTR("The field of view of the camera."),
       {CoreInfo::FloatType}},
      {"OffsetX",
       CBCCSTR("The horizontal offset of the viewport."),
       {CoreInfo::IntType}},
      {"OffsetY",
       CBCCSTR("The vertical offset of the viewport."),
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _width = int(value.payload.intValue);
      break;
    case 1:
      _height = int(value.payload.intValue);
      break;
    case 2:
      _near = float(value.payload.floatValue);
      break;
    case 3:
      _far = float(value.payload.floatValue);
      break;
    case 4:
      _fov = float(value.payload.floatValue);
      break;
    case 5:
      _offsetX = int(value.payload.intValue);
      break;
    case 6:
      _offsetY = int(value.payload.intValue);
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
      return Var(_near);
    case 3:
      return Var(_far);
    case 4:
      return Var(_fov);
    case 5:
      return Var(_offsetX);
    case 6:
      return Var(_offsetY);
    default:
      throw InvalidParameterIndex();
    }
  }

  void warmup(CBContext *context) {
    _bgfx_context = referenceVariable(context, "GFX.Context");
  }

  void cleanup() {
    if (_bgfx_context) {
      releaseVariable(_bgfx_context);
      _bgfx_context = nullptr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
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

    Context *ctx =
        reinterpret_cast<Context *>(_bgfx_context->payload.objectValue);

    const auto currentView = ctx->currentView();

    float view[16];
    bx::Vec3 *bp = reinterpret_cast<bx::Vec3 *>(&position.payload.float3Value);
    bx::Vec3 *bt = reinterpret_cast<bx::Vec3 *>(&target.payload.float3Value);
    bx::mtxLookAt(view, *bp, *bt);

    float proj[16];
    bx::mtxProj(proj, _fov, float(_width) / float(_height), _near, _far,
                bgfx::getCaps()->homogeneousDepth);

    if constexpr (CurrentRenderer == Renderer::OpenGL) {
      // workaround for flipped Y render to textures
      if (currentView > 0) {
        proj[5] = -proj[5];
        proj[8] = -proj[8];
        proj[9] = -proj[9];
      }
    }

    bgfx::setViewTransform(currentView, view, proj);
    bgfx::setViewRect(currentView, uint16_t(_offsetX), uint16_t(_offsetY),
                      uint16_t(_width), uint16_t(_height));

    return input;
  }
};

struct Draw : public BaseConsumer {
  // a matrix (in the form of 4 float4s)
  // or multiple matrices (will draw multiple times, instanced TODO)
  static inline Type InputSeqType = Type::SeqOf(CoreInfo::Float4SeqType);
  static inline Types InputTypes{{CoreInfo::Float4SeqType, InputSeqType}};

  static CBTypesInfo inputTypes() { return InputTypes; }
  static CBTypesInfo outputTypes() { return InputTypes; }

  // keep in mind that bgfx does its own sorting, so we don't need to make this
  // block way too smart
  static inline Parameters params{
      {"Shader",
       CBCCSTR("The shader program to use for this draw."),
       {ShaderHandle::VarType, CoreInfo::NoneType}},
      {"Model",
       CBCCSTR("The model to draw."),
       {ModelHandle::VarType, CoreInfo::NoneType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _shader = value;
      break;
    case 1:
      _model = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _shader;
    case 1:
      return _model;
    default:
      return Var::Empty;
    }
  }

  ParamVar _shader{};
  ParamVar _model{};
  CBVar *_bgfx_context{nullptr};
  std::array<CBExposedTypeInfo, 2> _exposing;

  CBExposedTypesInfo requiredVariables() {
    int idx = -1;
    if (_shader.isVariable()) {
      idx++;
      _exposing[idx].name = _shader.variableName();
      _exposing[idx].help = CBCCSTR("The required shader.");
      _exposing[idx].exposedType = ShaderHandle::ObjType;
    }
    if (_model.isVariable()) {
      idx++;
      _exposing[idx].name = _model.variableName();
      _exposing[idx].help = CBCCSTR("The required model.");
      _exposing[idx].exposedType = ModelHandle::ObjType;
    }
    if (idx == -1) {
      return {};
    } else {
      return {_exposing.data(), uint32_t(idx + 1), 0};
    }
  }

  void warmup(CBContext *context) {
    _shader.warmup(context);
    _model.warmup(context);
    _bgfx_context = referenceVariable(context, "GFX.Context");
  }

  void cleanup() {
    _shader.cleanup();
    _model.cleanup();
    if (_bgfx_context) {
      releaseVariable(_bgfx_context);
      _bgfx_context = nullptr;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.inputType.seqTypes.elements[0].basicType == CBType::Seq) {
      // TODO
      OVERRIDE_ACTIVATE(data, activate);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return data.inputType;
  }

  CBVar activateSingle(CBContext *context, const CBVar &input) {
    auto shader =
        reinterpret_cast<ShaderHandle *>(_shader.get().payload.objectValue);
    auto model =
        reinterpret_cast<ModelHandle *>(_model.get().payload.objectValue);
    auto *ctx = reinterpret_cast<Context *>(_bgfx_context->payload.objectValue);

    if (input.payload.seqValue.len != 4) {
      throw ActivationError("Invalid Matrix4x4 input, should Float4 x 4.");
    }

    float mat[16];
    memcpy(&mat[0], &input.payload.seqValue.elements[0].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&mat[4], &input.payload.seqValue.elements[1].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&mat[8], &input.payload.seqValue.elements[2].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&mat[12], &input.payload.seqValue.elements[3].payload.float4Value,
           sizeof(float) * 4);
    bgfx::setTransform(mat);

    std::visit(
        [](auto &m) {
          // Set vertex and index buffer.
          bgfx::setVertexBuffer(0, m.vb);
          bgfx::setIndexBuffer(m.ib);
        },
        model->model);

    auto culling = BGFX_STATE_CULL_CW;
    const auto currentView = ctx->currentView();

    if constexpr (CurrentRenderer == Renderer::OpenGL) {
      // workaround for flipped Y render to textures
      if (currentView > 0) {
        culling = BGFX_STATE_CULL_CCW;
      }
    }

    // set state, (it's auto reset after submit)
    uint64_t state = 0 | BGFX_STATE_WRITE_R | BGFX_STATE_WRITE_G |
                     BGFX_STATE_WRITE_B | BGFX_STATE_WRITE_A |
                     BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | culling;
    bgfx::setState(state);

    // Submit primitive for rendering to the current view.
    bgfx::submit(currentView, shader->handle);

    return input;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    throw ActivationError("Invalid activation path.");
  }
};

struct RenderTexture : public BaseConsumer {
  // to make it simple our render textures are always 16bit linear
  // TODO we share same size/formats depth buffers, expose only color part

  static inline Parameters params{
      {"Width",
       CBCCSTR("The width of the texture to render."),
       {CoreInfo::IntType}},
      {"Height",
       CBCCSTR("The height of the texture to render."),
       {CoreInfo::IntType}},
      {"Contents",
       CBCCSTR("The blocks expressing the contents to render."),
       {CoreInfo::BlocksOrNone}}};
  static CBParametersInfo parameters() { return params; }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return Texture::TextureHandleType; }

  BlocksVar _blocks;
  int _width{256};
  int _height{256};
  bgfx::FrameBufferHandle _framebuffer = BGFX_INVALID_HANDLE;
  CBVar *_bgfx_context{nullptr};
  bgfx::ViewId _viewId;

  Texture *_texture{nullptr}; // the color part we expose
  Texture _depth{};

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
    default:
      throw InvalidParameterIndex();
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("GFX Blocks cannot be used on a worker thread (e.g. "
                         "within an Await block)");
    }
    _blocks.compose(data);
    return Texture::TextureHandleType;
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

    _bgfx_context = referenceVariable(context, "GFX.Context");
    assert(_bgfx_context->valueType == CBType::Object);
    Context *ctx =
        reinterpret_cast<Context *>(_bgfx_context->payload.objectValue);
    _viewId = ctx->nextView();
    bgfx::setViewFrameBuffer(_viewId, _framebuffer);
    bgfx::setViewRect(_viewId, 0, 0, _width, _height);
    bgfx::setViewClear(_viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030FF,
                       1.0f, 0);

    _blocks.warmup(context);
  }

  void cleanup() {
    _blocks.cleanup();

    if (_bgfx_context) {
      releaseVariable(_bgfx_context);
      _bgfx_context = nullptr;
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
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    Context *ctx =
        reinterpret_cast<Context *>(_bgfx_context->payload.objectValue);

    // push _viewId
    ctx->push(_viewId);
    DEFER({
      ctx->pop();
      assert(ctx->index() == _viewId);
    });

    // Touch _viewId
    bgfx::touch(_viewId);

    // activate the blocks and render
    CBVar output{};
    _blocks.activate(context, input, output);

    return Texture::Var.Get(_texture);
  }
};

#ifdef __EMSCRIPTEN__
EM_JS(bool, cb_emscripten_show_xr_dialog, (), {
  console.log("Stop here and ask for XR permissions...");
  return false;
});
#endif

struct RenderXR : public RenderTexture {
#ifdef __EMSCRIPTEN__
  static EM_BOOL on_button_click(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData) {
    webxr_request_session();
    return true;
  }
#endif

  void warmup(CBContext *context) {
#ifdef __EMSCRIPTEN__
    emscripten_set_click_callback("#chainblocks-webxr-dialog-ok", (void*)this, true, on_button_click);
    if(!cb_emscripten_show_xr_dialog()) {
      LOG(WARNING) << "Failed to enable XR session.";
    }
#endif
  }

  void cleanup() {
#ifdef __EMSCRIPTEN__
    webxr_request_exit();
#endif
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
  REGISTER_CBLOCK("GFX.Draw", Draw);
  REGISTER_CBLOCK("GFX.RenderTexture", RenderTexture);
}
}; // namespace BGFX

#ifdef CB_INTERNAL_TESTS
#include "bgfx_tests.cpp"
#endif

#if defined(_WIN32) || defined(__linux__)
#include "SDL_syswm.h"
namespace BGFX {
#if defined(_WIN32)
HWND SDL_GetNativeWindowPtr(SDL_Window *window)
#elif defined(__linux__)
void *SDL_GetNativeWindowPtr(SDL_Window *window)
#endif
{
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
#endif
}
} // namespace BGFX
#endif
