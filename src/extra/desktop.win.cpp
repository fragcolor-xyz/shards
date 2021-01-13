/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#define WINVER 0x0602
#include "desktop.capture.win.hpp"
#include "desktop.hpp"
#include <Windows.h>
#include <psapi.h>

#ifndef NDEBUG
#define DBG_CHECK_OFF 1
#else
#define DBG_CHECK_OFF 0
#endif

#define DBG_CHECK(_expr_)                                                      \
  if (DBG_CHECK_OFF) {                                                         \
    assert((_expr_));                                                          \
  } else {                                                                     \
    (_expr_);                                                                  \
  }

namespace Desktop {
template <> HWND WindowBase<HWND>::WindowDefault() { return NULL; }

class WindowWindows : public WindowBase<HWND> {
protected:
  std::vector<wchar_t> _wTitle;
  std::vector<wchar_t> _wClass;

public:
  void setParam(int index, const CBVar &value) override {
    WindowBase<HWND>::setParam(index, value);

    switch (index) {
    case 0:
      _wTitle.resize(
          MultiByteToWideChar(CP_UTF8, 0, _winName.c_str(), -1, 0, 0));
      MultiByteToWideChar(CP_UTF8, 0, _winName.c_str(), -1, &_wTitle[0],
                          _wTitle.size());
      break;
    case 1:
      _wClass.resize(
          MultiByteToWideChar(CP_UTF8, 0, _winClass.c_str(), -1, 0, 0));
      MultiByteToWideChar(CP_UTF8, 0, _winClass.c_str(), -1, &_wClass[0],
                          _wClass.size());
      break;
    default:
      break;
    }
  }
};

class HasWindow : public WindowWindows {
public:
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_window || !IsWindow(_window)) {
      if (_winClass.size() > 0) {
        _window = FindWindowW(&_wClass[0], &_wTitle[0]);
      } else {
        _window = FindWindowW(NULL, &_wTitle[0]);
      }
    }

    return _window ? Var::True : Var::False;
  }
};

class WaitWindow : public WindowWindows {
public:
  static CBTypesInfo outputTypes() { return Globals::windowType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    while (!_window || !IsWindow(_window)) {
      if (_winClass.size() > 0) {
        _window = FindWindowW(&_wClass[0], &_wTitle[0]);
      } else {
        _window = FindWindowW(NULL, &_wTitle[0]);
      }

      if (!_window) {
        CB_SUSPEND(context, 0.1);
      }
    }

    return Var::Object(_window, CoreCC, windowCC);
  }
};

static HWND AsHWND(const CBVar &var) {
  if (var.valueType == Object && var.payload.objectVendorId == CoreCC &&
      var.payload.objectTypeId == windowCC) {
    return reinterpret_cast<HWND>(var.payload.objectValue);
  }
  return NULL;
}

struct PID : public PIDBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      DWORD pid;
      GetWindowThreadProcessId(hwnd, &pid);
      return Var(int64_t(pid));
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
  };
};

struct IsForeground : public ActiveBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      auto currentForeground = GetForegroundWindow();
      return hwnd == currentForeground ? Var::True : Var::False;
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
  };
};

struct SetForeground : public WinOpBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      auto foreground = GetForegroundWindow();
      if (foreground != hwnd) {
        auto target = GetWindowThreadProcessId(foreground, NULL);
        auto current = GetWindowThreadProcessId(hwnd, NULL);
        AttachThreadInput(current, target, TRUE);

        BOOL res;
        res = SetForegroundWindow(hwnd);
        assert(res);
        BringWindowToTop(hwnd);
        SetFocus(hwnd);

        AttachThreadInput(current, target, FALSE);
      }
      return input;
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
  };
};

struct NotForeground : public ActiveBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      auto currentForeground = GetForegroundWindow();
      return hwnd != currentForeground ? Var::True : Var::False;
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
  };
};

struct Resize : public ResizeWindowBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      RECT rect;
      DBG_CHECK(GetWindowRect(hwnd, &rect));
      DBG_CHECK(::MoveWindow(hwnd, rect.left, rect.top, _width, _height, TRUE));
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }

    return input;
  };
};

struct Move : public MoveWindowBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      RECT rect;
      DBG_CHECK(GetWindowRect(hwnd, &rect));
      auto width = rect.right - rect.left;
      auto height = rect.bottom - rect.top;
      DBG_CHECK(::MoveWindow(hwnd, _x, _y, width, height, TRUE));
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }

    return input;
  };
};

struct Bounds : public SizeBase {
  // gets client size, work area
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      RECT r;
      DBG_CHECK(GetClientRect(hwnd, &r));
      int64_t width = r.right - r.left;
      int64_t height = r.bottom - r.top;
      return Var(width, height);
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
  };
};

struct Size : public SizeBase {
  // gets window size, including OS bar
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      RECT r;
      DBG_CHECK(GetWindowRect(hwnd, &r));
      int64_t width = r.right - r.left;
      int64_t height = r.bottom - r.top;
      return Var(width, height);
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
  };
};

struct SetBorderless : public WinOpBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      LONG lStyle = GetWindowLong(hwnd, GWL_STYLE);
      lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE |
                  WS_SYSMENU);
      SetWindowLong(hwnd, GWL_STYLE, lStyle);
      LONG lExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
      lExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
      SetWindowLong(hwnd, GWL_EXSTYLE, lExStyle);
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
    return input;
  };
};
// TODO add Unset

struct SetClickthrough : public WinOpBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      LONG lStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
      // Make it layered if it's not
      if (!(lStyle & WS_EX_LAYERED)) {
        lStyle |= WS_EX_LAYERED;
      }
      // Make transparent
      lStyle |= WS_EX_TRANSPARENT;
      SetWindowLong(hwnd, GWL_EXSTYLE, lStyle);
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
    return input;
  };
};

struct UnsetClickthrough : public WinOpBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      LONG lStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
      if (lStyle & WS_EX_TRANSPARENT) {
        lStyle &= ~WS_EX_TRANSPARENT;
      }
      SetWindowLong(hwnd, GWL_EXSTYLE, lStyle);
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
    return input;
  };
};

struct SetTopmost : public WinOpBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
    return input;
  };
};

struct UnsetTopmost : public WinOpBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }
    return input;
  };
};

struct SetTitle : public SetTitleBase {
  std::vector<wchar_t> _wTitle;

  void setParam(int index, const CBVar &value) override {
    SetTitleBase::setParam(index, value);

    _wTitle.resize(MultiByteToWideChar(CP_UTF8, 0, _title.c_str(), -1, 0, 0));
    MultiByteToWideChar(CP_UTF8, 0, _title.c_str(), -1, &_wTitle[0],
                        _wTitle.size());
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto hwnd = AsHWND(input);
    if (hwnd) {
      SetWindowTextW(hwnd, &_wTitle[0]);
    } else {
      throw ActivationError("Input object was not a Desktop's window!");
    }

    return input;
  };
};

struct PixelBase {
  std::string variableName;

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Window",
      CBCCSTR("The window variable name to use as coordinate origin."),
      Globals::windowVarOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  ExposedInfo exposedInfo;
  CBVar *windowVar = nullptr;

  void cleanup() {
    if (windowVar) {
      releaseVariable(windowVar);
      windowVar = nullptr;
    }
  }

  CBExposedTypesInfo requiredVariables() {
    if (variableName.size() == 0) {
      return {};
    } else {
      return CBExposedTypesInfo(exposedInfo);
    }
  }

  virtual void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      if (value.valueType == ContextVar) {
        variableName = value.payload.stringValue;
        exposedInfo = ExposedInfo(ExposedInfo::Variable(
            variableName.c_str(), CBCCSTR("The window to use as origin."),
            Globals::windowType));
      } else {
        variableName.clear();
      }
      windowVar = nullptr;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      if (variableName.size() == 0) {
        return Var::Empty;
      } else {
        CBVar v{};
        v.valueType = CBType::ContextVar;
        v.payload.stringValue = variableName.c_str();
        return v;
      }
    default:
      break;
    }
    return res;
  }

  static inline std::vector<std::unique_ptr<DXGIDesktopCapture>> Grabbers;

  static DXGIDesktopCapture *findOrCreate(int x, int y) {
    for (auto &grabber : Grabbers) {
      if (x >= grabber->left() && x < grabber->right() && y >= grabber->top() &&
          y < grabber->bottom()) {
        return grabber.get();
      }
    }

    auto info = DXGIDesktopCapture::FindScreen(x, y, 1, 1);
    if (info.adapter != nullptr) {
      auto grabber = new DXGIDesktopCapture(info);
      Grabbers.emplace_back(grabber);
      info.adapter->Release();
      info.output->Release();

      // should we also start the global loop?
      if (Grabbers.size() == 1) {
        chainblocks::registerRunLoopCallback("fragcolor.desktop.grabber", [] {
          int len = Grabbers.size();
          for (int i = len - 1; i >= 0; i--) {
            auto &grabber = Grabbers[i];
            auto state = grabber->capture();
            if (state != DXGIDesktopCapture::Timeout &&
                state != DXGIDesktopCapture::Normal) {
              Grabbers.erase(Grabbers.begin() + i);
            } else {
              grabber->update();
            }
          }
        });

        chainblocks::registerExitCallback("fragcolor.desktop.grabber",
                                          [] { Grabbers.clear(); });
      }

      return grabber;
    }

    return nullptr;
  }

  DXGIDesktopCapture *preActivate(CBContext *context, int &x, int &y) {
    if (windowVar == nullptr && variableName.size() != 0) {
      windowVar = referenceVariable(context, variableName.c_str());
    }

    if (windowVar) {
      auto wnd = AsHWND(*windowVar);
      if (!wnd) {
        throw ActivationError("Input object was not a Desktop's window!");
      }
      POINT po;
      po.x = x;
      po.y = y;
      ClientToScreen(wnd, &po);
      LogicalToPhysicalPoint(wnd, &po);
      x = po.x;
      y = po.y;
    }

    return findOrCreate(x, y);
  }
};

struct Pixel : public PixelBase {
  static CBTypesInfo inputTypes() { return CoreInfo::Int2Type; }
  static CBTypesInfo outputTypes() { return CoreInfo::ColorType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    int x = input.payload.int2Value[0];
    int y = input.payload.int2Value[1];
    auto grabber = preActivate(context, x, y);
    if (grabber) {
      auto img = grabber->image();
      // properly clamp the single pixel
      x = std::max(0, x);
      y = std::max(0, y);
      x = std::min(grabber->width() - 1, x);
      y = std::min(grabber->height() - 1, y);
      // grab from the bgra image
      auto pindex = ((grabber->width() * y) + x) * 4;
      CBColor pixel = {img[pindex + 2], img[pindex + 1], img[pindex], 255};
      return Var(pixel);
    } else {
      CBColor pixel = {0, 0, 0, 255};
      return Var(pixel);
    }
  }
};

struct Pixels : public PixelBase {
  static CBTypesInfo inputTypes() { return CoreInfo::Int4Type; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  CBVar _output;

  Pixels() {
    _output = CBVar(); // zero it
    _output.valueType = Image;
    _output.payload.imageValue.channels = 4;
    _output.payload.imageValue.flags = 0;
  }

  ~Pixels() {
    if (_output.payload.imageValue.data) {
      delete[] _output.payload.imageValue.data;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    int left = input.payload.int4Value[0];
    int top = input.payload.int4Value[1];
    const int right = input.payload.int4Value[2];
    const int bottom = input.payload.int4Value[3];
    int w = right - left;
    int h = bottom - top;

    auto grabber = preActivate(context, left, top);
    if (grabber) {
      auto img = grabber->image();
      // adjust, due to virtual coordinates of multiple screens
      auto x = left - grabber->left();
      auto y = top - grabber->top();

      // properly clamp the capture area
      x = std::clamp(x, 0, grabber->width() - w);
      y = std::clamp(y, 0, grabber->height() - h);
      w = std::clamp(w, 2, grabber->width());
      h = std::clamp(h, 2, grabber->height());

      // ensure storage for the image
      if (w != _output.payload.imageValue.width ||
          h != _output.payload.imageValue.height) {
        // recreate output buffer
        if (_output.payload.imageValue.data) {
          delete[] _output.payload.imageValue.data;
        }
        auto nbytes = 4 * w * h;
        _output.payload.imageValue.data = new uint8_t[nbytes];
        _output.payload.imageValue.width = w;
        _output.payload.imageValue.height = h;
      }

      // grab from the bgra image
      auto yindex = y;
      for (auto i = 0; i < h; i++) {
        auto xindex = x;
        for (auto j = 0; j < w; j++) {
          auto sindex = ((grabber->width() * yindex) + xindex) * 4;
          auto dindex = ((w * i) + j) * 4;
          _output.payload.imageValue.data[dindex + 2] = img[sindex + 0];
          _output.payload.imageValue.data[dindex + 1] = img[sindex + 1];
          _output.payload.imageValue.data[dindex + 0] = img[sindex + 2];
          _output.payload.imageValue.data[dindex + 3] = img[sindex + 3];
          xindex++;
        }
        yindex++;
      }

      return _output;
    } else {
      throw ActivationError("Failed to grab screen.");
    }
  }
};

struct WaitKeyEvent : public WaitKeyEventBase {
  struct KeyboardHookState {
    int refCount = 0;
    HHOOK hook{};
    std::vector<WaitKeyEvent *> receivers;
  };

  static inline thread_local KeyboardHookState *hookState = nullptr;

  bool attached = false;
  std::deque<CBVar> events;

  void keyboardEvent(int state, int vkCode) {
    events.push_back(Var(state, vkCode));
  }

  void cleanup() {
    if (attached && hookState) {
      auto selfIt = std::find(hookState->receivers.begin(),
                              hookState->receivers.end(), this);
      hookState->receivers.erase(selfIt);
      hookState->refCount--;
      if (hookState->refCount == 0) {
        auto res = UnhookWindowsHookEx(hookState->hook);
        assert(res);
        delete hookState;
        hookState = nullptr;
      }
      attached = false;
    }
  }

  static LRESULT __attribute__((stdcall))
  Hookproc(int nCode, WPARAM wParam, LPARAM lParam) {
    auto wp = int(wParam);
    auto lp = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
    for (auto &rcvr : hookState->receivers) {
      if (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN) {
        rcvr->keyboardEvent(0, lp->vkCode);
      } else if (wp == WM_KEYUP || wp == WM_SYSKEYUP) {
        rcvr->keyboardEvent(1, lp->vkCode);
      }
    }
    return CallNextHookEx(hookState->hook, nCode, wParam, lParam);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!attached) {
      if (!hookState) {
        hookState = new KeyboardHookState();
        hookState->refCount++;
        hookState->receivers.push_back(this);
        hookState->hook = SetWindowsHookEx(WH_KEYBOARD_LL, Hookproc, NULL, 0);
        assert(hookState->hook);
        // also add a empty os pump
        registerRunLoopCallback("fragcolor.desktop.keys-ospump", [] {
          MSG msg;
          PeekMessage(&msg, 0, 0, 0, 0);
        });
      } else {
        hookState->refCount++;
        hookState->receivers.push_back(this);
      }
      attached = true;
    }

    while (events.empty()) {
      // Wait for events
      CB_SUSPEND(context, 0);
    }

    auto event = events.front();
    events.pop_front();
    return event;
  }
};

struct SendKeyEvent : public SendKeyEventBase {
  CBVar *_window = nullptr;

  void cleanup() {
    if (_window) {
      releaseVariable(_window);
      _window = nullptr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto state = input.payload.int2Value[0];
    UINT vkCode = input.payload.int2Value[1];

    if (_windowVarName.size() > 0 && !_window) {
      _window = referenceVariable(context, _windowVarName.c_str());
    }

    if (_windowVarName.size() > 0) {
      auto window = AsHWND(*_window);
      if (!window) {
        throw ActivationError("Input object was not a Desktop's window!");
      }

      auto lparam = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC) << 16;
      lparam = lparam | (state == 0) ? 0x00000001 : 0xC0000001;
      PostMessage(window, state == 0 ? WM_KEYDOWN : WM_KEYUP, vkCode, lparam);
    } else {
      INPUT keyboardEvent;
      keyboardEvent.type = INPUT_KEYBOARD;
      keyboardEvent.ki.wVk = vkCode;
      keyboardEvent.ki.wScan = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
      keyboardEvent.ki.dwFlags = 0;
      if (state == 1) { // release
        keyboardEvent.ki.dwFlags = keyboardEvent.ki.dwFlags | KEYEVENTF_KEYUP;
      }
      keyboardEvent.ki.dwExtraInfo = GetMessageExtraInfo();
      SendInput(1, &keyboardEvent, sizeof(keyboardEvent));
    }
    return input;
  }
};

struct GetMousePos : public MousePosBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    POINT p;

    if (!GetPhysicalCursorPos(&p)) {
      throw ActivationError("GetPhysicalCursorPos failed.");
    }

    auto wnd = AsHWND(_window.get());
    if (wnd) {
      // both return bool but checking might be not necessary
      // if we fail hopefully the struct is unchanged
      PhysicalToLogicalPoint(wnd, &p);
      ScreenToClient(wnd, &p);
    }

    return Var((int64_t)p.x, (int64_t)p.y);
  }
};

struct SetMousePos : public MousePosBase {
  static CBTypesInfo inputTypes() { return CoreInfo::Int2Type; }

  CBVar activate(CBContext *context, const CBVar &input) {
    POINT p;
    p.x = input.payload.int2Value[0];
    p.y = input.payload.int2Value[1];

    auto wnd = AsHWND(_window.get());
    if (wnd) {
      // both return bool but checking might be not necessary
      // if we fail hopefully the struct is unchanged
      ClientToScreen(wnd, &p);
      LogicalToPhysicalPoint(wnd, &p);
    }

    if (!SetPhysicalCursorPos(p.x, p.y)) {
      throw ActivationError("SetPhysicalCursorPos failed.");
    }

    return input;
  }
};

struct Tap : public MousePosBase {
  typedef BOOL (*InitializeTouchInjectionProc)(UINT32 maxCount, DWORD dwMode);
  typedef BOOL (*InjectTouchInputProc)(UINT32 count,
                                       const POINTER_TOUCH_INFO *contacts);
  struct GlobalInjector {
    static inline InjectTouchInputProc InjectTouch;
    GlobalInjector() {
      // win7 compatibility
      auto user32 = LoadLibraryA("User32.dll");
      assert(user32);
      auto touchInit = (InitializeTouchInjectionProc)GetProcAddress(
          user32, "InitializeTouchInjection");
      if (touchInit) {
        DBG_CHECK(touchInit(10, TOUCH_FEEDBACK_NONE));
        InjectTouch =
            (InjectTouchInputProc)GetProcAddress(user32, "InjectTouchInput");
      } else {
        InjectTouch = nullptr;
      }
    }
  };

  static inline GlobalInjector *sInjectorState = nullptr;

  void setup() {
    // we cannot use static inline with value type..
    // runtime error with 32 bit builds...
    if (!sInjectorState)
      sInjectorState = new GlobalInjector();
  }

  bool _delays = true;
  bool _longTap = false;

  static inline ParamsInfo params = ParamsInfo(
      MousePosBase::params,
      ParamsInfo::Param("Long",
                        CBCCSTR("A big delay will be injected after tap down "
                                "to simulate a long tap."),
                        CoreInfo::BoolType),
      ParamsInfo::Param(
          "Natural",
          CBCCSTR("Small pauses will be injected after tap events down & up."),
          CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CoreInfo::Int2Type; }

  void setParam(int index, const CBVar &value) {
    if (index == 0)
      MousePosBase::setParam(index, value);
    else {
      if (index == 1)
        _longTap = value.payload.boolValue;
      else // 2
        _delays = value.payload.boolValue;
    }
  }

  CBVar getParam(int index) {
    if (index == 0)
      return MousePosBase::getParam(index);
    else {
      if (index == 1)
        return Var(_longTap);
      else
        return Var(_delays);
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!GlobalInjector::InjectTouch)
      return input;

    POINTER_TOUCH_INFO pinfo;
    memset(&pinfo, 0x0, sizeof(POINTER_TOUCH_INFO));
    pinfo.pointerInfo.pointerType = PT_TOUCH;
    pinfo.pointerInfo.pointerId = 0;
    pinfo.touchFlags = TOUCH_FLAG_NONE;
    pinfo.touchMask =
        TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
    pinfo.orientation = 90; // 0~359 afaik
    pinfo.pressure = 1024;  // 0~1024 afaik

    auto wnd = AsHWND(_window.get());
    if (wnd) {
      POINT p;
      p.x = input.payload.int2Value[0];
      p.y = input.payload.int2Value[1];
      ClientToScreen(wnd, &p);
      LogicalToPhysicalPoint(wnd, &p);
      pinfo.pointerInfo.ptPixelLocation.x = p.x;
      pinfo.pointerInfo.ptPixelLocation.y = p.y;
    } else {
      pinfo.pointerInfo.ptPixelLocation.x = input.payload.int2Value[0];
      pinfo.pointerInfo.ptPixelLocation.y = input.payload.int2Value[1];
    }

    pinfo.rcContact.left = pinfo.pointerInfo.ptPixelLocation.x - 2;
    pinfo.rcContact.right = pinfo.pointerInfo.ptPixelLocation.x + 2;
    pinfo.rcContact.top = pinfo.pointerInfo.ptPixelLocation.y - 2;
    pinfo.rcContact.bottom = pinfo.pointerInfo.ptPixelLocation.y + 2;

    // down
    pinfo.pointerInfo.pointerFlags =
        POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
    if (!GlobalInjector::InjectTouch(1, &pinfo)) {
      LOG(ERROR) << "InjectTouchInput (down) error: 0x" << std::hex
                 << GetLastError() << std::dec;
      throw ActivationError("InjectTouchInput failed.");
    }

    if (_delays) {
      CB_SUSPEND(context, 0.05);
    }

    if (_longTap) {
      // we need to send updates or it will fail, a simple sleep is not enough
      for (auto i = 0; i < 10; i++) {
        pinfo.pointerInfo.pointerFlags =
            POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
        if (!GlobalInjector::InjectTouch(1, &pinfo)) {
          LOG(ERROR) << "InjectTouchInput (down) error: 0x" << std::hex
                     << GetLastError() << std::dec;
          throw ActivationError("InjectTouchInput failed.");
        }
        CB_SUSPEND(context, 0.1);
      }
    }

    // up
    pinfo.pointerInfo.pointerFlags = POINTER_FLAG_UP;
    if (!GlobalInjector::InjectTouch(1, &pinfo)) {
      LOG(ERROR) << "InjectTouchInput (up) error: 0x" << std::hex
                 << GetLastError() << std::dec;
      throw ActivationError("InjectTouchInput failed.");
    }

    if (_delays) {
      CB_SUSPEND(context, 0.05);
    }

    return input;
  }
};

template <DWORD MBD, DWORD MBU> struct Click : public MousePosBase {
  bool _delays = true;

  static inline ParamsInfo params = ParamsInfo(
      MousePosBase::params,
      ParamsInfo::Param(
          "Natural",
          CBCCSTR(
              "Small pauses will be injected after click events down & up."),
          CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CoreInfo::Int2Type; }

  void setParam(int index, const CBVar &value) {
    if (index == 0)
      MousePosBase::setParam(index, value);
    else {
      _delays = value.payload.boolValue;
    }
  }

  CBVar getParam(int index) {
    if (index == 0)
      return MousePosBase::getParam(index);
    else {
      return Var(_delays);
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    INPUT event;
    event.type = INPUT_MOUSE;
    event.mi.mouseData = 0;
    event.mi.dwFlags = 0;
    event.ki.dwExtraInfo = GetMessageExtraInfo();
    event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

    auto wnd = AsHWND(_window.get());
    if (wnd) {
      POINT p;
      p.x = input.payload.int2Value[0];
      p.y = input.payload.int2Value[1];
      ClientToScreen(wnd, &p);
      LogicalToPhysicalPoint(wnd, &p);
      event.mi.dx = p.x;
      event.mi.dy = p.y;
    } else {
      event.mi.dx = input.payload.int2Value[0];
      event.mi.dy = input.payload.int2Value[1];
    }

    event.mi.dx = (event.mi.dx * 65536) / GetSystemMetrics(SM_CXSCREEN);
    event.mi.dy = (event.mi.dy * 65536) / GetSystemMetrics(SM_CYSCREEN);

    // down
    event.mi.dwFlags = event.mi.dwFlags | MBD;
    if (!SendInput(1, &event, sizeof(INPUT))) {
      LOG(ERROR) << "SendInput (down) error: 0x" << std::hex << GetLastError()
                 << std::dec;
      throw ActivationError("LeftClick failed.");
    }

    if (_delays) {
      CB_SUSPEND(context, 0.05);
    }

    // up
    event.mi.dwFlags = event.mi.dwFlags | MBU;
    if (!SendInput(1, &event, sizeof(INPUT))) {
      LOG(ERROR) << "SendInput (up) error: 0x" << std::hex << GetLastError()
                 << std::dec;
      throw ActivationError("LeftClick failed.");
    }

    if (_delays) {
      CB_SUSPEND(context, 0.05);
    }

    return input;
  }
};

typedef Click<MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP> LeftClick;
typedef Click<MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP> RightClick;
typedef Click<MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP> MiddleClick;

struct CursorBitmap {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  HDC _hdcMem;
  HBITMAP _bitmap;
  CBImage _img;

  CursorBitmap() {
    auto iconw = GetSystemMetrics(SM_CXICON);
    auto iconh = GetSystemMetrics(SM_CYICON);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = iconw;
    info.bmiHeader.biHeight = iconh;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    info.bmiHeader.biCompression = BI_RGB;

    _img.channels = 3;
    _img.flags = 0;
    _img.width = iconw;
    _img.height = iconh;

    _hdcMem = CreateCompatibleDC(0);
    _bitmap = CreateDIBSection(_hdcMem, &info, DIB_RGB_COLORS,
                               (void **)&_img.data, 0, 0);
    assert(_img.data != nullptr);
  }

  ~CursorBitmap() {
    DeleteObject(_bitmap);
    DeleteDC(_hdcMem);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto oldObj = SelectObject(_hdcMem, _bitmap);
    CURSORINFO ci;
    ci.cbSize = sizeof(ci);
    // reset to 0s the bitmap, cos the next call won't fill all!
    memset(_img.data, 0x0, _img.width * _img.height * _img.channels);
    if (GetCursorInfo(&ci)) {
      DrawIconEx(_hdcMem, 0, 0, ci.hCursor, 0, 0, 0, 0, DI_NORMAL);
    }
    SelectObject(_hdcMem, oldObj);
    return Var(_img);
  }
};

extern "C" NTSYSAPI NTSTATUS NTAPI NtSetTimerResolution(
    ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);

struct SetTimerResolution {
  static CBTypesInfo inputTypes() { return CoreInfo::IntType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }

  void cleanup() {
    ULONG tmp;
    NtSetTimerResolution(0, FALSE, &tmp);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // in 100 ns, 5000 = 0.5 milliseconds
    // max 156250 , default OS seems
    ULONG res = (ULONG)input.payload.intValue;
    ULONG tmp;

    if (NtSetTimerResolution(res, TRUE, &tmp)) {
      throw ActivationError("Failed to call NtSetTimerResolution - TRUE");
    }

    return Var((int64_t)tmp);
  }
};

struct LastInput : public LastInputBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    LASTINPUTINFO info;
    info.cbSize = sizeof(info);
    info.dwTime = 0;
    if (GetLastInputInfo(&info)) {
      auto tickCount = double(GetTickCount64());
      auto ticks = double(info.dwTime);
      auto diff = tickCount - ticks;
      return Var(diff / 1000.0);
    } else {
      throw ActivationError("GetLastInputInfo failed");
    }
  }
};
// TODO REFACTOR

RUNTIME_BLOCK(Desktop, HasWindow);
RUNTIME_BLOCK_cleanup(HasWindow);
RUNTIME_BLOCK_inputTypes(HasWindow);
RUNTIME_BLOCK_outputTypes(HasWindow);
RUNTIME_BLOCK_parameters(HasWindow);
RUNTIME_BLOCK_setParam(HasWindow);
RUNTIME_BLOCK_getParam(HasWindow);
RUNTIME_BLOCK_activate(HasWindow);
RUNTIME_BLOCK_END(HasWindow);

RUNTIME_BLOCK(Desktop, WaitWindow);
RUNTIME_BLOCK_cleanup(WaitWindow);
RUNTIME_BLOCK_inputTypes(WaitWindow);
RUNTIME_BLOCK_outputTypes(WaitWindow);
RUNTIME_BLOCK_parameters(WaitWindow);
RUNTIME_BLOCK_setParam(WaitWindow);
RUNTIME_BLOCK_getParam(WaitWindow);
RUNTIME_BLOCK_activate(WaitWindow);
RUNTIME_BLOCK_END(WaitWindow);

RUNTIME_BLOCK(Desktop, PID);
RUNTIME_BLOCK_inputTypes(PID);
RUNTIME_BLOCK_outputTypes(PID);
RUNTIME_BLOCK_activate(PID);
RUNTIME_BLOCK_END(PID);

RUNTIME_BLOCK(Desktop, IsForeground);
RUNTIME_BLOCK_inputTypes(IsForeground);
RUNTIME_BLOCK_outputTypes(IsForeground);
RUNTIME_BLOCK_activate(IsForeground);
RUNTIME_BLOCK_END(IsForeground);

RUNTIME_BLOCK(Desktop, SetForeground);
RUNTIME_BLOCK_inputTypes(SetForeground);
RUNTIME_BLOCK_outputTypes(SetForeground);
RUNTIME_BLOCK_activate(SetForeground);
RUNTIME_BLOCK_END(SetForeground);

RUNTIME_BLOCK(Desktop, NotForeground);
RUNTIME_BLOCK_inputTypes(NotForeground);
RUNTIME_BLOCK_outputTypes(NotForeground);
RUNTIME_BLOCK_activate(NotForeground);
RUNTIME_BLOCK_END(NotForeground);

RUNTIME_BLOCK(Desktop, Resize);
RUNTIME_BLOCK_inputTypes(Resize);
RUNTIME_BLOCK_outputTypes(Resize);
RUNTIME_BLOCK_parameters(Resize);
RUNTIME_BLOCK_setParam(Resize);
RUNTIME_BLOCK_getParam(Resize);
RUNTIME_BLOCK_activate(Resize);
RUNTIME_BLOCK_END(Resize);

RUNTIME_BLOCK(Desktop, Move);
RUNTIME_BLOCK_inputTypes(Move);
RUNTIME_BLOCK_outputTypes(Move);
RUNTIME_BLOCK_parameters(Move);
RUNTIME_BLOCK_setParam(Move);
RUNTIME_BLOCK_getParam(Move);
RUNTIME_BLOCK_activate(Move);
RUNTIME_BLOCK_END(Move);

RUNTIME_BLOCK(Desktop, Size);
RUNTIME_BLOCK_inputTypes(Size);
RUNTIME_BLOCK_outputTypes(Size);
RUNTIME_BLOCK_activate(Size);
RUNTIME_BLOCK_END(Size);

RUNTIME_BLOCK(Desktop, Bounds);
RUNTIME_BLOCK_inputTypes(Bounds);
RUNTIME_BLOCK_outputTypes(Bounds);
RUNTIME_BLOCK_activate(Bounds);
RUNTIME_BLOCK_END(Bounds);

RUNTIME_BLOCK(Desktop, SetBorderless);
RUNTIME_BLOCK_inputTypes(SetBorderless);
RUNTIME_BLOCK_outputTypes(SetBorderless);
RUNTIME_BLOCK_activate(SetBorderless);
RUNTIME_BLOCK_END(SetBorderless);

RUNTIME_BLOCK(Desktop, SetClickthrough);
RUNTIME_BLOCK_inputTypes(SetClickthrough);
RUNTIME_BLOCK_outputTypes(SetClickthrough);
RUNTIME_BLOCK_activate(SetClickthrough);
RUNTIME_BLOCK_END(SetClickthrough);

RUNTIME_BLOCK(Desktop, UnsetClickthrough);
RUNTIME_BLOCK_inputTypes(UnsetClickthrough);
RUNTIME_BLOCK_outputTypes(UnsetClickthrough);
RUNTIME_BLOCK_activate(UnsetClickthrough);
RUNTIME_BLOCK_END(UnsetClickthrough);

RUNTIME_BLOCK(Desktop, SetTopmost);
RUNTIME_BLOCK_inputTypes(SetTopmost);
RUNTIME_BLOCK_outputTypes(SetTopmost);
RUNTIME_BLOCK_activate(SetTopmost);
RUNTIME_BLOCK_END(SetTopmost);

RUNTIME_BLOCK(Desktop, UnsetTopmost);
RUNTIME_BLOCK_inputTypes(UnsetTopmost);
RUNTIME_BLOCK_outputTypes(UnsetTopmost);
RUNTIME_BLOCK_activate(UnsetTopmost);
RUNTIME_BLOCK_END(UnsetTopmost);

RUNTIME_BLOCK(Desktop, SetTitle);
RUNTIME_BLOCK_inputTypes(SetTitle);
RUNTIME_BLOCK_outputTypes(SetTitle);
RUNTIME_BLOCK_parameters(SetTitle);
RUNTIME_BLOCK_setParam(SetTitle);
RUNTIME_BLOCK_getParam(SetTitle);
RUNTIME_BLOCK_activate(SetTitle);
RUNTIME_BLOCK_END(SetTitle);

RUNTIME_BLOCK(Desktop, Pixel);
RUNTIME_BLOCK_inputTypes(Pixel);
RUNTIME_BLOCK_outputTypes(Pixel);
RUNTIME_BLOCK_activate(Pixel);
RUNTIME_BLOCK_cleanup(Pixel);
RUNTIME_BLOCK_parameters(Pixel);
RUNTIME_BLOCK_setParam(Pixel);
RUNTIME_BLOCK_getParam(Pixel);
RUNTIME_BLOCK_requiredVariables(Pixel);
RUNTIME_BLOCK_END(Pixel);

RUNTIME_BLOCK(Desktop, Pixels);
RUNTIME_BLOCK_inputTypes(Pixels);
RUNTIME_BLOCK_outputTypes(Pixels);
RUNTIME_BLOCK_activate(Pixels);
RUNTIME_BLOCK_cleanup(Pixels);
RUNTIME_BLOCK_parameters(Pixels);
RUNTIME_BLOCK_setParam(Pixels);
RUNTIME_BLOCK_getParam(Pixels);
RUNTIME_BLOCK_requiredVariables(Pixels);
RUNTIME_BLOCK_END(Pixels);

RUNTIME_BLOCK(Desktop, WaitKeyEvent);
RUNTIME_BLOCK_inputTypes(WaitKeyEvent);
RUNTIME_BLOCK_outputTypes(WaitKeyEvent);
RUNTIME_BLOCK_activate(WaitKeyEvent);
RUNTIME_BLOCK_cleanup(WaitKeyEvent);
RUNTIME_BLOCK_help(WaitKeyEvent);
RUNTIME_BLOCK_END(WaitKeyEvent);

RUNTIME_BLOCK(Desktop, SendKeyEvent);
RUNTIME_BLOCK_inputTypes(SendKeyEvent);
RUNTIME_BLOCK_outputTypes(SendKeyEvent);
RUNTIME_BLOCK_parameters(SendKeyEvent);
RUNTIME_BLOCK_setParam(SendKeyEvent);
RUNTIME_BLOCK_getParam(SendKeyEvent);
RUNTIME_BLOCK_activate(SendKeyEvent);
RUNTIME_BLOCK_cleanup(SendKeyEvent);
RUNTIME_BLOCK_help(SendKeyEvent);
RUNTIME_BLOCK_requiredVariables(SendKeyEvent);
RUNTIME_BLOCK_END(SendKeyEvent);

RUNTIME_BLOCK(Desktop, GetMousePos);
RUNTIME_BLOCK_cleanup(GetMousePos);
RUNTIME_BLOCK_warmup(GetMousePos);
RUNTIME_BLOCK_inputTypes(GetMousePos);
RUNTIME_BLOCK_outputTypes(GetMousePos);
RUNTIME_BLOCK_parameters(GetMousePos);
RUNTIME_BLOCK_setParam(GetMousePos);
RUNTIME_BLOCK_getParam(GetMousePos);
RUNTIME_BLOCK_activate(GetMousePos);
RUNTIME_BLOCK_requiredVariables(GetMousePos);
RUNTIME_BLOCK_END(GetMousePos);

RUNTIME_BLOCK(Desktop, SetMousePos);
RUNTIME_BLOCK_cleanup(SetMousePos);
RUNTIME_BLOCK_warmup(SetMousePos);
RUNTIME_BLOCK_inputTypes(SetMousePos);
RUNTIME_BLOCK_outputTypes(SetMousePos);
RUNTIME_BLOCK_parameters(SetMousePos);
RUNTIME_BLOCK_setParam(SetMousePos);
RUNTIME_BLOCK_getParam(SetMousePos);
RUNTIME_BLOCK_activate(SetMousePos);
RUNTIME_BLOCK_requiredVariables(SetMousePos);
RUNTIME_BLOCK_END(SetMousePos);

RUNTIME_BLOCK(Desktop, Tap);
RUNTIME_BLOCK_setup(Tap);
RUNTIME_BLOCK_cleanup(Tap);
RUNTIME_BLOCK_warmup(Tap);
RUNTIME_BLOCK_inputTypes(Tap);
RUNTIME_BLOCK_outputTypes(Tap);
RUNTIME_BLOCK_parameters(Tap);
RUNTIME_BLOCK_setParam(Tap);
RUNTIME_BLOCK_getParam(Tap);
RUNTIME_BLOCK_activate(Tap);
RUNTIME_BLOCK_requiredVariables(Tap);
RUNTIME_BLOCK_END(Tap);

RUNTIME_BLOCK(Desktop, LeftClick);
RUNTIME_BLOCK_cleanup(LeftClick);
RUNTIME_BLOCK_warmup(LeftClick);
RUNTIME_BLOCK_inputTypes(LeftClick);
RUNTIME_BLOCK_outputTypes(LeftClick);
RUNTIME_BLOCK_parameters(LeftClick);
RUNTIME_BLOCK_setParam(LeftClick);
RUNTIME_BLOCK_getParam(LeftClick);
RUNTIME_BLOCK_activate(LeftClick);
RUNTIME_BLOCK_requiredVariables(LeftClick);
RUNTIME_BLOCK_END(LeftClick);

RUNTIME_BLOCK(Desktop, RightClick);
RUNTIME_BLOCK_cleanup(RightClick);
RUNTIME_BLOCK_warmup(RightClick);
RUNTIME_BLOCK_inputTypes(RightClick);
RUNTIME_BLOCK_outputTypes(RightClick);
RUNTIME_BLOCK_parameters(RightClick);
RUNTIME_BLOCK_setParam(RightClick);
RUNTIME_BLOCK_getParam(RightClick);
RUNTIME_BLOCK_activate(RightClick);
RUNTIME_BLOCK_requiredVariables(RightClick);
RUNTIME_BLOCK_END(RightClick);

RUNTIME_BLOCK(Desktop, MiddleClick);
RUNTIME_BLOCK_cleanup(MiddleClick);
RUNTIME_BLOCK_warmup(MiddleClick);
RUNTIME_BLOCK_inputTypes(MiddleClick);
RUNTIME_BLOCK_outputTypes(MiddleClick);
RUNTIME_BLOCK_parameters(MiddleClick);
RUNTIME_BLOCK_setParam(MiddleClick);
RUNTIME_BLOCK_getParam(MiddleClick);
RUNTIME_BLOCK_activate(MiddleClick);
RUNTIME_BLOCK_requiredVariables(MiddleClick);
RUNTIME_BLOCK_END(MiddleClick);

RUNTIME_BLOCK(Desktop, CursorBitmap);
RUNTIME_BLOCK_inputTypes(CursorBitmap);
RUNTIME_BLOCK_outputTypes(CursorBitmap);
RUNTIME_BLOCK_activate(CursorBitmap);
RUNTIME_BLOCK_END(CursorBitmap);

RUNTIME_BLOCK(Desktop, SetTimerResolution);
RUNTIME_BLOCK_inputTypes(SetTimerResolution);
RUNTIME_BLOCK_outputTypes(SetTimerResolution);
RUNTIME_BLOCK_activate(SetTimerResolution);
RUNTIME_BLOCK_cleanup(SetTimerResolution);
RUNTIME_BLOCK_END(SetTimerResolution);

void registerDesktopBlocks() {
  REGISTER_BLOCK(Desktop, HasWindow);
  REGISTER_BLOCK(Desktop, WaitWindow);
  REGISTER_BLOCK(Desktop, PID);
  REGISTER_BLOCK(Desktop, IsForeground);
  REGISTER_BLOCK(Desktop, SetForeground);
  REGISTER_BLOCK(Desktop, NotForeground);
  REGISTER_BLOCK(Desktop, Resize);
  REGISTER_BLOCK(Desktop, Move);
  REGISTER_BLOCK(Desktop, Size);
  REGISTER_BLOCK(Desktop, Bounds);
  REGISTER_BLOCK(Desktop, SetBorderless);
  REGISTER_BLOCK(Desktop, SetClickthrough);
  REGISTER_BLOCK(Desktop, UnsetClickthrough);
  REGISTER_BLOCK(Desktop, SetTopmost);
  REGISTER_BLOCK(Desktop, UnsetTopmost);
  REGISTER_BLOCK(Desktop, SetTitle);
  REGISTER_BLOCK(Desktop, Pixel);
  REGISTER_BLOCK(Desktop, WaitKeyEvent);
  REGISTER_BLOCK(Desktop, SendKeyEvent);
  REGISTER_BLOCK(Desktop, GetMousePos);
  REGISTER_BLOCK(Desktop, SetMousePos);
  REGISTER_BLOCK(Desktop, Tap);
  REGISTER_BLOCK(Desktop, LeftClick);
  REGISTER_BLOCK(Desktop, RightClick);
  REGISTER_BLOCK(Desktop, MiddleClick);
  REGISTER_BLOCK(Desktop, CursorBitmap);
  REGISTER_BLOCK(Desktop, SetTimerResolution);
  REGISTER_CBLOCK("Desktop.LastInput", LastInput);
}
}; // namespace Desktop
