/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "./bgfx.hpp"
#include "./imgui.hpp"
#include "SDL.h"
#include <cstdlib>

/*
TODO

(GFX.Camera ...)
(GFX.Model :Layout [] :Vertices [] :Indices []) >= .mymodel
(GFX.Draw :Model .mymodel :Shader .myshader), input world matrices (multiple =
possible instanced rendering)
*/

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

  static inline ExposedInfo requiredInfo = ExposedInfo(
      ExposedInfo::Variable("GFX.Context", "The BGFX Context.", Context::Info));

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(requiredInfo);
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
      {"Title", "The title of the window to create.", {CoreInfo::StringType}},
      {"Width", "The width of the window to create", {CoreInfo::IntType}},
      {"Height", "The height of the window to create.", {CoreInfo::IntType}},
      {"Contents", "The contents of this window.", {CoreInfo::BlocksOrNone}}};

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
  Context _bgfx_context{};
  chainblocks::ImGui::Context _imgui_context{};
  int32_t _wheelScroll = 0;
  bool _bgfxInit{false};

  // TODO thread_local? anyway sort multiple threads
  static inline std::vector<SDL_Event> sdlEvents;

  CBTypeInfo compose(const CBInstanceData &data) {
    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
        throw CBException("GFX.MainWindow must be unique, found another use!");
      }
    }

    CBInstanceData dataCopy = data;
    arrayPush(dataCopy.shared,
              ExposedInfo::ProtectedVariable("GFX.CurrentWindow",
                                             "The exposed SDL window.",
                                             BaseConsumer::windowType));
    arrayPush(dataCopy.shared,
              ExposedInfo::ProtectedVariable("GFX.Context", "The BGFX Context.",
                                             Context::Info));
    arrayPush(dataCopy.shared, ExposedInfo::ProtectedVariable(
                                   "GUI.Context", "The ImGui Context.",
                                   chainblocks::ImGui::Context::Info));
    _blocks.compose(dataCopy);

    return data.inputType;
  }

  void cleanup() {
    // cleanup before releasing the resources
    _blocks.cleanup();

    // _imgui_context.Reset();

    if (_bgfxInit) {
      imguiDestroy();
      bgfx::shutdown();
    }

    unregisterRunLoopCallback("fragcolor.bgfx.ospump");

#ifdef __APPLE__
    if (_metalView) {
      SDL_Metal_DestroyView(_metalView);
      _metalView = nullptr;
    }
#endif

    if (_window) {
      SDL_DestroyWindow(_window);
      SDL_Quit();
    }

    if (_sdlWinVar) {
      if (_sdlWinVar->refcount > 1) {
        throw CBException(
            "MainWindow: Found a dangling reference to GFX.CurrentWindow.");
      }
      memset(_sdlWinVar, 0x0, sizeof(CBVar));
      _sdlWinVar = nullptr;
    }

    if (_bgfxCtx) {
      if (_bgfxCtx->refcount > 1) {
        throw CBException(
            "MainWindow: Found a dangling reference to GFX.Context.");
      }
      memset(_bgfxCtx, 0x0, sizeof(CBVar));
      _bgfxCtx = nullptr;
    }

    if (_imguiCtx) {
      if (_imguiCtx->refcount > 1) {
        throw CBException(
            "MainWindow: Found a dangling reference to GUI.Context.");
      }
      memset(_imguiCtx, 0x0, sizeof(CBVar));
      _imguiCtx = nullptr;
    }

    if (_nativeWnd) {
      releaseVariable(_nativeWnd);
      _nativeWnd = nullptr;
    }

    _window = nullptr;
    _sysWnd = nullptr;
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
    auto initErr = SDL_Init(SDL_INIT_EVENTS);
    if (initErr != 0) {
      LOG(ERROR) << "Failed to initialize SDL " << SDL_GetError();
      throw ActivationError("Failed to initialize SDL");
    }

    registerRunLoopCallback("fragcolor.bgfx.ospump", [] {
      sdlEvents.clear();
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        sdlEvents.push_back(event);
      }
    });

    bgfx::Init initInfo{};

    // try to see if global native window is set
    _nativeWnd = referenceVariable(context, "fragcolor.bgfx.nativewindow");
    if (_nativeWnd->valueType == Object &&
        _nativeWnd->payload.objectVendorId == CoreCC &&
        _nativeWnd->payload.objectTypeId == BgfxNativeWindowCC) {
      _sysWnd = decltype(_sysWnd)(_nativeWnd->payload.objectValue);
      // TODO SDL_CreateWindowFrom to enable inputs etc...
      // specially for iOS thing is that we pass context as variable, not a
      // window object we might need 2 variables in the end
    } else {
      Uint32 flags = SDL_WINDOW_SHOWN;
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
#ifdef SDL_VIDEO_DRIVER_UIKIT
      _metalView = SDL_Metal_CreateView(_window);
      _sysWnd = SDL_Metal_GetLayer(_metalView);
#else
      _metalView = SDL_Metal_CreateView(_window);
      _sysWnd = SDL_Metal_GetLayer(_metalView);
#endif
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

    initInfo.platformData.nwh = _sysWnd;
    initInfo.resolution.width = _width;
    initInfo.resolution.height = _height;
    initInfo.resolution.reset = BGFX_RESET_VSYNC;
    if (!bgfx::init(initInfo)) {
      throw ActivationError("Failed to initialize BGFX");
    } else {
      _bgfxInit = true;
    }

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
    *_sdlWinVar = Var::Object(_sysWnd, CoreCC, windowCC);
    *_bgfxCtx = Var::Object(&_bgfx_context, CoreCC, BgfxContextCC);
    *_imguiCtx = Var::Object(&_imgui_context, CoreCC,
                             chainblocks::ImGui::ImGuiContextCC);

    // init blocks after we initialize the system
    _blocks.warmup(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // Set them always as they might override each other during the chain
    *_sdlWinVar = Var::Object(_sysWnd, CoreCC, windowCC);
    *_bgfxCtx = Var::Object(&_bgfx_context, CoreCC, BgfxContextCC);
    *_imguiCtx = Var::Object(&_imgui_context, CoreCC,
                             chainblocks::ImGui::ImGuiContextCC);

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
  Texture *_texture{nullptr};

  void cleanup() {
    if (_texture) {
      Texture::Var.Release(_texture);
      _texture = nullptr;
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }

  static CBTypesInfo outputTypes() { return Texture::TextureHandleType; }

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
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::R8);
          break;
        case 2:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::RG8);
          break;
        case 3:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::RGB8);
          break;
        case 4:
          _texture->handle =
              bgfx::createTexture2D(_texture->width, _texture->height, false, 1,
                                    bgfx::TextureFormat::RGBA8);
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
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters f_v_params{
      {"VertexShader",
       "The vertex shader bytecode.",
       {CoreInfo::BytesType, CoreInfo::BytesVarType}},
      {"PixelShader",
       "The pixel shader bytecode.",
       {CoreInfo::BytesType, CoreInfo::BytesVarType}}};

  static inline Parameters c_params{
      {"ComputeShader",
       "The compute shader bytecode.",
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

struct Model {
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
  static CBTypesInfo outputTypes() { return ModelHandle::ModelHandleType; }

  static inline Parameters params{
      {"Layout", "The vertex layout of this model.", {VertexAttributeSeqType}},
      {"Dynamic",
       "If the model is dynamic and will be optimized to change as often as "
       "every frame.",
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

    return ModelHandle::ModelHandleType;
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
      case 'v':
        vertices = v;
        break;
      case 'I':
      case 'i':
        indices = v;
        break;
      default:
        break;
      }
    }

    if (vertices.valueType != Seq || indices.valueType != Seq)
      throw ActivationError("GFX.Model input table is invalid");

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
      if (compressed) {
        const static Var max{UINT16_MAX, UINT16_MAX, UINT16_MAX};
        if (selems[i] < min || selems[i] > max) {
          throw ActivationError("Vertex index out of range");
        }
        const uint16_t t[] = {uint16_t(selems[i].payload.int3Value[0]),
                              uint16_t(selems[i].payload.int3Value[1]),
                              uint16_t(selems[i].payload.int3Value[2])};
        memcpy(ibuffer->data + offset, t, sizeof(uint16_t) * 3);
        offset += sizeof(uint16_t) * 3;
      } else {
        const static Var max{INT32_MAX, INT32_MAX, INT32_MAX};
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

void registerBGFXBlocks() {
  REGISTER_CBLOCK("GFX.MainWindow", MainWindow);
  REGISTER_CBLOCK("GFX.Texture2D", Texture2D);
  using GraphicsShader = Shader<'g'>;
  REGISTER_CBLOCK("GFX.Shader", GraphicsShader);
  using ComputeShader = Shader<'c'>;
  REGISTER_CBLOCK("GFX.ComputeShader", ComputeShader);
  REGISTER_CBLOCK("GFX.Model", Model);
}
}; // namespace BGFX

#ifdef CB_INTERNAL_TESTS

#ifdef CHECK
#undef CHECK
#endif

#include <catch2/catch_all.hpp>

namespace chainblocks {
namespace BGFX_Tests {
void testVertexAttribute() {
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Position) ==
          bgfx::Attrib::Position);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Normal) ==
          bgfx::Attrib::Normal);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Tangent) ==
          bgfx::Attrib::Tangent);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Bitangent) ==
          bgfx::Attrib::Bitangent);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Color0) ==
          bgfx::Attrib::Color0);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Color1) ==
          bgfx::Attrib::Color1);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Color2) ==
          bgfx::Attrib::Color2);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Color3) ==
          bgfx::Attrib::Color3);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Indices) ==
          bgfx::Attrib::Indices);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Weight) ==
          bgfx::Attrib::Weight);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord0) ==
          bgfx::Attrib::TexCoord0);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord1) ==
          bgfx::Attrib::TexCoord1);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord2) ==
          bgfx::Attrib::TexCoord2);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord3) ==
          bgfx::Attrib::TexCoord3);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord4) ==
          bgfx::Attrib::TexCoord4);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord5) ==
          bgfx::Attrib::TexCoord5);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord6) ==
          bgfx::Attrib::TexCoord6);
  REQUIRE(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::TexCoord7) ==
          bgfx::Attrib::TexCoord7);
  REQUIRE_THROWS(BGFX::Model::toBgfx(BGFX::Model::VertexAttribute::Skip) ==
                 bgfx::Attrib::TexCoord7);
}

void testModel() {
  chainblocks::Globals::RootPath = "./";
  registerCoreBlocks();

  std::vector<Var> layout = {
      Var::Enum(BGFX::Model::VertexAttribute::Position, CoreCC, 'gfxV'),
      Var::Enum(BGFX::Model::VertexAttribute::Color0, CoreCC, 'gfxV')};

  SECTION("Working") {
    std::vector<Var> cubeVertices = {
        Var(-1.0, 1.0, 1.0),   Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(cubeVertices)
                                .block("Set", "cube", "Vertices")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout)));
    auto node = CBNode::make();
    node->schedule(chain);
    auto count = 100;
    while (count--) {
      REQUIRE(node->tick()); // false is chain errors happened
      chainblocks::sleep(0.016);
    }
  }

  SECTION("Fail-Activate1") {
    std::vector<Var> cubeVertices = {
        Var(1.0, 1.0),         Var(0xff000000),      Var(1.0, 1.0, 1.0),
        Var(0xff0000ff),       Var(-1.0, -1.0, 1.0), Var(0xff00ff00),
        Var(1.0, -1.0, 1.0),   Var(0xff00ffff),      Var(-1.0, 1.0, -1.0),
        Var(0xffff0000),       Var(1.0, 1.0, -1.0),  Var(0xffff00ff),
        Var(-1.0, -1.0, -1.0), Var(0xffffff00),      Var(1.0, -1.0, -1.0),
        Var(0xffffffff),
    };
    std::vector<Var> cubeIndices = {
        Var(0, 1, 2), Var(1, 3, 2), Var(4, 6, 5), Var(5, 6, 7),
        Var(0, 2, 4), Var(4, 2, 6), Var(1, 5, 3), Var(5, 7, 3),
        Var(0, 4, 1), Var(4, 5, 1), Var(2, 3, 6), Var(6, 3, 7),
    };
    auto chain = chainblocks::Chain("test-chain")
                     .looped(true)
                     .block("GFX.MainWindow", "MainWindow", Var::Any, Var::Any,
                            Blocks()
                                .let(cubeVertices)
                                .block("Set", "cube", "Vertices")
                                .let(cubeIndices)
                                .block("Set", "cube", "Indices")
                                .block("Get", "cube")
                                .block("GFX.Model", Var(layout)));
    auto node = CBNode::make();
    node->schedule(chain);
    REQUIRE_FALSE(node->tick()); // false is chain errors happened
    auto errors = node->errors();
    LOG(ERROR) << errors[0];
    REQUIRE(errors[0] == "Invalid vertex buffer element type");
  }
} // namespace BGFX_Tests

} // namespace BGFX_Tests
} // namespace chainblocks
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
