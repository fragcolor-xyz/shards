#include "./bgfx.hpp"
#include "./imgui.hpp"
#include "bgfx.hpp"
#include "blocks/shared.hpp"
#include "SDL.h"
#include "SDL_syswm.h"

using namespace chainblocks;

namespace BGFX {
struct Base {
  CBVar *_bgfxCtx;
};

constexpr uint32_t windowCC = 'hwnd';
const static TypeInfo windowType = TypeInfo::Object(FragCC, windowCC);
const static TypesInfo windowInfo = TypesInfo(windowType);


struct BaseConsumer : public Base {
  static inline ExposedInfo consumedInfo = ExposedInfo(ExposedInfo::Variable(
      "BGFX.Context", "The BGFX Context.", CBTypeInfo(Context::Info)));

  CBExposedTypesInfo consumedVariables() {
    return CBExposedTypesInfo(consumedInfo);
  }
};

struct BaseWindow : public Base {
#ifdef _WIN32
  HWND _sysWnd = nullptr;
#elif defined(__APPLE__)
  NSWindow *_sysWnd = nullptr;
#elif defined(__linux__)
  void *_sysWnd = nullptr;
#endif

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Title", "The title of the window to create.",
                        CBTypesInfo(SharedTypes::strInfo)),
      ParamsInfo::Param("Width", "The width of the window to create",
                        CBTypesInfo(SharedTypes::intInfo)),
      ParamsInfo::Param("Height", "The height of the window to create.",
                        CBTypesInfo(SharedTypes::intInfo)));

  static CBTypesInfo inputTypes() {
    return CBTypesInfo((SharedTypes::anyInfo));
  }

  static CBTypesInfo outputTypes() {
    return CBTypesInfo((SharedTypes::anyInfo));
  }

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  bool _initDone = false;
  std::string _title;
  int _width = 1024;
  int _height = 768;
  SDL_Window *_window = nullptr;
  CBVar *_sdlWinVar = nullptr;
  CBVar *_imguiCtx = nullptr;

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      break;
    case 1:
      _width = value.payload.intValue;
      break;
    case 2:
      _height = value.payload.intValue;
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
    default:
      return Empty;
    }
  }
};

struct MainWindow : public BaseWindow {
  static inline ExposedInfo exposedInfo = ExposedInfo(
      ExposedInfo::Variable("BGFX.CurrentWindow", "The exposed SDL window.",
                            CBTypeInfo(windowInfo)),
      ExposedInfo::Variable("BGFX.Context", "The BGFX Context.",
                            CBTypeInfo(Context::Info)),
      ExposedInfo::Variable("ImGui.Context", "The ImGui Context.",
                            CBTypeInfo(chainblocks::ImGui::Context::Info)));

  CBExposedTypesInfo exposedVariables() {
    return CBExposedTypesInfo(exposedInfo);
  }

  Context _bgfx_context{};
  chainblocks::ImGui::Context _imgui_context{};
  int32_t _wheelScroll = 0;
  static inline std::vector<SDL_Event> sdlEvents;

  void cleanup() {
    _imgui_context.Reset();

    if (_initDone) {
      imguiDestroy();
      bgfx::shutdown();
      unregisterRunLoopCallback("fragcolor.bgfx.ospump");
      SDL_DestroyWindow(_window);
      SDL_Quit();
      _window = nullptr;
      _sdlWinVar = nullptr;
      _bgfxCtx = nullptr;
      _sysWnd = nullptr;
      _initDone = false;
      _wheelScroll = 0;
    }
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    // Make sure MainWindow is UNIQUE
    for (auto i = 0; i < stbds_arrlen(consumableVariables); i++) {
      if (strcmp(consumableVariables[i].name, "BGFX.Context") == 0) {
        throw CBException("BGFX.MainWindow must be unique, found another use!");
      }
    }
    return inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_initDone) {
      auto initErr = SDL_Init(0);
      if (initErr != 0) {
        LOG(ERROR) << "Failed to initialize SDL " << SDL_GetError();
        throw CBException("Failed to initialize SDL");
      }

      registerRunLoopCallback("fragcolor.bgfx.ospump", [] {
        sdlEvents.clear();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          sdlEvents.push_back(event);
        }
      });

      SDL_SysWMinfo winInfo{};
      SDL_version sdlVer{};
      Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
      _window =
          SDL_CreateWindow(_title.c_str(), SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, _width, _height, flags);
      SDL_VERSION(&sdlVer);
      winInfo.version = sdlVer;
      if (!SDL_GetWindowWMInfo(_window, &winInfo)) {
        throw CBException("Failed to call SDL_GetWindowWMInfo");
      }

      bgfx::Init initInfo{};
#ifdef __APPLE__
      _sysWnd = winInfo.info.cocoa.window;
#elif defined(_WIN32)
      _sysWnd = winInfo.info.win.window;
#elif defined(__linux__)
      _sysWnd = (void *)winInfo.info.x11.window;
#endif
      initInfo.platformData.nwh = _sysWnd;

      initInfo.resolution.width = _width;
      initInfo.resolution.height = _height;
      initInfo.resolution.reset = BGFX_RESET_VSYNC;
      if (!bgfx::init(initInfo)) {
        throw CBException("Failed to initialize BGFX");
      }

      imguiCreate();

      bgfx::setViewRect(0, 0, 0, _width, _height);
      bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030FF,
                         1.0f, 0);

      _sdlWinVar = contextVariable(context, "BGFX.CurrentWindow");
      _bgfxCtx = contextVariable(context, "BGFX.Context");
      _imguiCtx = contextVariable(context, "ImGui.Context");

      _initDone = true;
    }

    // Set them always as they might override each other during the chain
    *_sdlWinVar = Var::Object(_sysWnd, FragCC, windowCC);
    *_bgfxCtx = Var::Object(&_bgfx_context, FragCC, BgfxContextCC);
    *_imguiCtx = Var::Object(&_imgui_context, FragCC,
                             chainblocks::ImGui::ImGuiContextCC);

    // Touch view 0
    bgfx::touch(0);

    _imgui_context.Set();

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

    // find mouse wheel events
    for (auto &event : sdlEvents) {
      if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.direction == SDL_MOUSEWHEEL_NORMAL)
          _wheelScroll += event.wheel.y;
        else
          _wheelScroll -= event.wheel.y;
      }
    }

    imguiBeginFrame(mouseX, mouseY, imbtns, _wheelScroll, _width, _height);

    return input;
  }
};

struct Window : public BaseWindow {
  bgfx::FrameBufferHandle _frameBuffer = BGFX_INVALID_HANDLE;
  bgfx::ViewId _viewId; // todo manage
  chainblocks::ImGui::Context _imgui_context{};
  int32_t _wheelScroll = 0;

  void cleanup() {
    _imgui_context.Reset();

    if (_initDone) {
      if (bgfx::isValid(_frameBuffer)) {
        bgfx::destroy(_frameBuffer);
      }
      SDL_DestroyWindow(_window);
      _window = nullptr;
      _sdlWinVar = nullptr;
      _sysWnd = nullptr;
      _initDone = false;
      _wheelScroll = 0;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_initDone) {
      SDL_SysWMinfo winInfo{};
      SDL_version sdlVer{};
      Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
      _window =
          SDL_CreateWindow(_title.c_str(), SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, _width, _height, flags);
      SDL_VERSION(&sdlVer);
      winInfo.version = sdlVer;
      if (!SDL_GetWindowWMInfo(_window, &winInfo)) {
        throw CBException("Failed to call SDL_GetWindowWMInfo");
      }

#ifdef __APPLE__
      _sysWnd = winInfo.info.cocoa.window;
#elif defined(_WIN32)
      _sysWnd = winInfo.info.win.window;
#endif
      _frameBuffer = bgfx::createFrameBuffer(_sysWnd, _width, _height);

      bgfx::setViewRect(_viewId, 0, 0, _width, _height);
      bgfx::setViewClear(_viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                         0x303030FF, 1.0f, 0);

      _sdlWinVar = contextVariable(context, "BGFX.CurrentWindow");
      _imguiCtx = contextVariable(context, "ImGui.Context");

      _initDone = true;
    }

    // Set them always as they might override each other during the chain
    *_sdlWinVar = Var::Object(_sysWnd, FragCC, windowCC);
    *_imguiCtx = Var::Object(&_imgui_context, FragCC,
                             chainblocks::ImGui::ImGuiContextCC);

    // Touch view 0
    bgfx::touch(_viewId);

    _imgui_context.Set();

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

    // find mouse wheel events
    for (auto &event : MainWindow::sdlEvents) {
      if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.direction == SDL_MOUSEWHEEL_NORMAL)
          _wheelScroll += event.wheel.y;
        else
          _wheelScroll -= event.wheel.y;
      }
    }

    imguiBeginFrame(mouseX, mouseY, imbtns, _wheelScroll, _width, _height,
                    _viewId);

    return input;
  }
};

struct Draw : public BaseConsumer {
  static CBTypesInfo inputTypes() {
    return CBTypesInfo((SharedTypes::anyInfo));
  }

  static CBTypesInfo outputTypes() {
    return CBTypesInfo((SharedTypes::anyInfo));
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    imguiEndFrame();
    bgfx::frame();
    return input;
  }
};

// Register
RUNTIME_BLOCK(BGFX, MainWindow);
RUNTIME_BLOCK_cleanup(MainWindow);
RUNTIME_BLOCK_inferTypes(MainWindow);
RUNTIME_BLOCK_exposedVariables(MainWindow);
RUNTIME_BLOCK_parameters(MainWindow);
RUNTIME_BLOCK_setParam(MainWindow);
RUNTIME_BLOCK_getParam(MainWindow);
RUNTIME_BLOCK_inputTypes(MainWindow);
RUNTIME_BLOCK_outputTypes(MainWindow);
RUNTIME_BLOCK_activate(MainWindow);
RUNTIME_BLOCK_END(MainWindow);

RUNTIME_BLOCK(BGFX, Draw);
RUNTIME_BLOCK_inputTypes(Draw);
RUNTIME_BLOCK_outputTypes(Draw);
RUNTIME_BLOCK_activate(Draw);
RUNTIME_BLOCK_END(Draw);

void registerBGFXBlocks() {
  REGISTER_BLOCK(BGFX, MainWindow);
  REGISTER_BLOCK(BGFX, Draw);
}
}; // namespace BGFX