/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreGraphics/CoreGraphics.h>

#include "shards/core/module.hpp"
#include "desktop.capture.mac.hpp"
#include "desktop.hpp"

#include <vector>
#include <memory>

using namespace shards;

namespace Desktop {

// Helper to convert NSWindow* to opaque pointer
template <> void* WindowBase<void*>::WindowDefault() { return nullptr; }

// =============================================================================
// Window Management Shards
// =============================================================================

class WindowMac : public WindowBase<void*> {
protected:
    OwnedVar _previousTitle;
    OwnedVar _previousClass;
    pid_t _cachedPID;

public:
    void cleanup(SHContext *context) {
        WindowBase::cleanup(context);
        _previousTitle = Var::Empty;
        _previousClass = Var::Empty;
        _cachedPID = 0;
    }

    void ensureParams() {
        auto &name = _winName.get();
        auto &class_ = _winClass.get();
        bool changed = name != _previousTitle || class_ != _previousClass;

        if (changed) {
            SHLOG_TRACE("Window title changed, re-evaluating.");
            _window = nullptr;
            _cachedPID = 0;

            _previousTitle = name;
            _previousClass = class_;

            if (name.valueType != SHType::String || SHSTRVIEW(name).empty()) {
                throw ActivationError("Window title must be set.");
            }
        }
    }

    bool findWindow() {
        if (_window != nullptr) {
            return true;
        }

        auto titleView = SHSTRVIEW(_winName.get());
        NSString *targetTitle = [NSString stringWithUTF8String:titleView.data()];

        // Use Accessibility API to find window
        NSArray<NSRunningApplication *> *apps = [[NSWorkspace sharedWorkspace] runningApplications];

        for (NSRunningApplication *app in apps) {
            if (app.activationPolicy != NSApplicationActivationPolicyRegular) {
                continue;
            }

            pid_t pid = app.processIdentifier;
            AXUIElementRef appElement = AXUIElementCreateApplication(pid);
            if (!appElement) {
                continue;
            }

            CFArrayRef windows = nullptr;
            AXError error = AXUIElementCopyAttributeValue(appElement, kAXWindowsAttribute, (CFTypeRef *)&windows);

            if (error == kAXErrorSuccess && windows) {
                CFIndex count = CFArrayGetCount(windows);

                for (CFIndex i = 0; i < count; i++) {
                    AXUIElementRef window = (AXUIElementRef)CFArrayGetValueAtIndex(windows, i);
                    CFStringRef title = nullptr;

                    error = AXUIElementCopyAttributeValue(window, kAXTitleAttribute, (CFTypeRef *)&title);

                    if (error == kAXErrorSuccess && title) {
                        NSString *windowTitle = (__bridge NSString *)title;

                        if ([windowTitle isEqualToString:targetTitle]) {
                            _window = (void *)CFRetain(window);
                            _cachedPID = pid;
                            CFRelease(title);
                            CFRelease(windows);
                            CFRelease(appElement);
                            return true;
                        }

                        CFRelease(title);
                    }
                }

                CFRelease(windows);
            }

            CFRelease(appElement);
        }

        return false;
    }

    AXUIElementRef getWindowElement() {
        return (AXUIElementRef)_window;
    }

    pid_t getWindowPID() {
        return _cachedPID;
    }
};

class HasWindow : public WindowMac {
public:
    static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

    SHVar activate(SHContext *context, const SHVar &input) {
        ensureParams();
        return findWindow() ? Var::True : Var::False;
    }
};

class WaitWindow : public WindowMac {
public:
    static SHTypesInfo outputTypes() { return Globals::windowType; }

    SHVar activate(SHContext *context, const SHVar &input) {
        ensureParams();

        while (!findWindow()) {
            SH_SUSPEND(context, 0.1);
        }

        return Var::Object(_window, CoreCC, windowCC);
    }
};

// Helper to get AXUIElementRef from window var
static AXUIElementRef AsAXWindow(const SHVar &var) {
    if (var.valueType == SHType::Object &&
        var.payload.objectVendorId == CoreCC &&
        var.payload.objectTypeId == windowCC) {
        return (AXUIElementRef)var.payload.objectValue;
    }
    return nullptr;
}

struct PID : public PIDBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            pid_t pid;
            AXError error = AXUIElementGetPid(window, &pid);
            if (error == kAXErrorSuccess) {
                return Var((int64_t)pid);
            } else {
                throw ActivationError("Failed to get window PID");
            }
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

struct IsForeground : public ActiveBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            pid_t pid;
            AXError error = AXUIElementGetPid(window, &pid);
            if (error != kAXErrorSuccess) {
                throw ActivationError("Failed to get window PID");
            }

            NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            return app.isActive ? Var::True : Var::False;
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

struct SetForeground : public WinOpBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            pid_t pid;
            AXError error = AXUIElementGetPid(window, &pid);
            if (error != kAXErrorSuccess) {
                throw ActivationError("Failed to get window PID");
            }

            NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            [app activateWithOptions:NSApplicationActivateAllWindows];

            // Also try to raise the specific window
            AXUIElementSetAttributeValue(window, kAXMainAttribute, kCFBooleanTrue);
            AXUIElementPerformAction(window, kAXRaiseAction);

            return input;
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

struct NotForeground : public ActiveBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            pid_t pid;
            AXError error = AXUIElementGetPid(window, &pid);
            if (error != kAXErrorSuccess) {
                throw ActivationError("Failed to get window PID");
            }

            NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            return app.isActive ? Var::False : Var::True;
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

struct Resize : public ResizeWindowBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            CGSize size = CGSizeMake(_width.payload.intValue, _height.payload.intValue);
            AXValueRef sizeValue = AXValueCreate((AXValueType)kAXValueCGSizeType, &size);

            AXError error = AXUIElementSetAttributeValue(window, kAXSizeAttribute, sizeValue);
            CFRelease(sizeValue);

            if (error != kAXErrorSuccess) {
                throw ActivationError("Failed to resize window");
            }

            return input;
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

struct Move : public MoveWindowBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            CGPoint position = CGPointMake(_x.payload.intValue, _y.payload.intValue);
            AXValueRef posValue = AXValueCreate((AXValueType)(AXValueType)kAXValueCGPointType, &position);

            AXError error = AXUIElementSetAttributeValue(window, kAXPositionAttribute, posValue);
            CFRelease(posValue);

            if (error != kAXErrorSuccess) {
                throw ActivationError("Failed to move window");
            }

            return input;
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

struct Bounds : public SizeBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            AXValueRef sizeValue = nullptr;
            AXError error = AXUIElementCopyAttributeValue(window, kAXSizeAttribute, (CFTypeRef *)&sizeValue);

            if (error == kAXErrorSuccess && sizeValue) {
                CGSize size;
                AXValueGetValue(sizeValue, (AXValueType)kAXValueCGSizeType, &size);
                CFRelease(sizeValue);

                return Var((int64_t)size.width, (int64_t)size.height);
            } else {
                throw ActivationError("Failed to get window bounds");
            }
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

struct WindowSize : public SizeBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            AXValueRef sizeValue = nullptr;
            AXError error = AXUIElementCopyAttributeValue(window, kAXSizeAttribute, (CFTypeRef *)&sizeValue);

            if (error == kAXErrorSuccess && sizeValue) {
                CGSize size;
                AXValueGetValue(sizeValue, (AXValueType)kAXValueCGSizeType, &size);
                CFRelease(sizeValue);

                return Var((int64_t)size.width, (int64_t)size.height);
            } else {
                throw ActivationError("Failed to get window size");
            }
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

struct SetTitle : public SetTitleBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        auto window = AsAXWindow(input);
        if (window) {
            auto titleView = SHSTRVIEW(_title);
            NSString *title = [NSString stringWithUTF8String:titleView.data()];
            CFStringRef titleRef = (__bridge CFStringRef)title;

            AXError error = AXUIElementSetAttributeValue(window, kAXTitleAttribute, titleRef);

            if (error != kAXErrorSuccess) {
                throw ActivationError("Failed to set window title");
            }

            return input;
        } else {
            throw ActivationError("Input object was not a Desktop window!");
        }
    }
};

// =============================================================================
// Screen Capture Shards
// =============================================================================

struct PixelBase {
    PARAM_PARAMVAR(_window, "Window", "The window variable name to use as coordinate origin.", {Globals::windowType, Globals::windowVarType, shards::CoreInfo::NoneType});
    PARAM_IMPL(PARAM_IMPL_FOR(_window));

    PARAM_REQUIRED_VARIABLES();
    SHTypeInfo compose(const SHInstanceData &data) {
        PARAM_COMPOSE_REQUIRED_VARIABLES(data);
        return CoreInfo::ColorType;
    }

    void warmup(SHContext *context) { PARAM_WARMUP(context); }
    void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

    static inline std::vector<std::unique_ptr<ScreenCaptureKitGrabber>> Grabbers;

    static ScreenCaptureKitGrabber *findOrCreate(int x, int y) {
        for (auto &grabber : Grabbers) {
            if (x >= grabber->left() && x < grabber->right() &&
                y >= grabber->top() && y < grabber->bottom()) {
                return grabber.get();
            }
        }

        CGDirectDisplayID displayID;
        CGRect bounds;
        if (ScreenCaptureKitGrabber::FindDisplay(x, y, displayID, bounds)) {
            SHLOG_INFO("Creating screen grabber for display {}", displayID);
            auto grabber = std::make_unique<ScreenCaptureKitGrabber>(bounds.origin.x, bounds.origin.y,
                                                                       bounds.size.width, bounds.size.height);
            auto ptr = grabber.get();
            Grabbers.push_back(std::move(grabber));
            return ptr;
        }

        SHLOG_ERROR("Failed to find display for pixel grabber");
        return nullptr;
    }

    ScreenCaptureKitGrabber *preActivate(SHContext *context, int &x, int &y) {
        auto &windowVar = _window.get();
        if (windowVar.valueType != SHType::None) {
            auto window = AsAXWindow(windowVar);
            if (!window) {
                throw ActivationError("Window parameter is not a valid Desktop window!");
            }

            // Get window position
            AXValueRef posValue = nullptr;
            AXError error = AXUIElementCopyAttributeValue(window, kAXPositionAttribute, (CFTypeRef *)&posValue);

            if (error == kAXErrorSuccess && posValue) {
                CGPoint position;
                AXValueGetValue(posValue, (AXValueType)kAXValueCGPointType, &position);
                CFRelease(posValue);

                x += position.x;
                y += position.y;
            }
        }

        return findOrCreate(x, y);
    }
};

struct CaptureFrame {
    static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
    static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

    SHVar activate(SHContext *context, const SHVar &input) {
        int len = PixelBase::Grabbers.size();
        for (int i = len - 1; i >= 0; i--) {
            auto &grabber = PixelBase::Grabbers[i];
            auto state = grabber->capture();
            if (state != ScreenCaptureKitGrabber::Timeout && state != ScreenCaptureKitGrabber::Normal) {
                PixelBase::Grabbers.erase(PixelBase::Grabbers.begin() + i);
            } else {
                grabber->update();
            }
        }
        return input;
    }
};

struct Pixel : public PixelBase {
    static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }
    static SHTypesInfo outputTypes() { return CoreInfo::ColorType; }

    SHVar activate(SHContext *context, const SHVar &input) {
        int x = input.payload.int2Value[0];
        int y = input.payload.int2Value[1];
        auto grabber = preActivate(context, x, y);

        if (grabber) {
            auto img = grabber->image();

            // Clamp coordinates
            x = std::max(0, std::min(x - grabber->left(), grabber->width() - 1));
            y = std::max(0, std::min(y - grabber->top(), grabber->height() - 1));

            // Get pixel from BGRA image
            auto pindex = ((grabber->width() * y) + x) * 4;
            SHColor pixel = {img[pindex + 2], img[pindex + 1], img[pindex], 255};
            return Var(pixel);
        } else {
            SHColor pixel = {0, 0, 0, 255};
            return Var(pixel);
        }
    }
};

struct Pixels : public PixelBase {
    static SHTypesInfo inputTypes() { return CoreInfo::Int4Type; }
    static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }

    OwnedVar _output;

    SHVar activate(SHContext *context, const SHVar &input) {
        int left = input.payload.int4Value[0];
        int top = input.payload.int4Value[1];
        const int right = input.payload.int4Value[2];
        const int bottom = input.payload.int4Value[3];
        int w = right - left;
        int h = bottom - top;

        auto grabber = preActivate(context, left, top);
        if (grabber) {
            auto img = grabber->image();

            // Adjust for display offset
            auto x = left - grabber->left();
            auto y = top - grabber->top();

            // Clamp capture area
            x = std::clamp(x, 0, grabber->width() - w);
            y = std::clamp(y, 0, grabber->height() - h);
            w = std::clamp(w, 2, grabber->width());
            h = std::clamp(h, 2, grabber->height());

            auto nbytes = 4 * w * h;
            _output = makeImage(w * h * nbytes);
            _output.payload.imageValue->width = w;
            _output.payload.imageValue->height = h;
            _output.payload.imageValue->channels = 4;
            _output.payload.imageValue->flags = 0;
            SHImage &outImage = *_output.payload.imageValue;

            // Copy from BGRA image to RGBA
            auto yindex = y;
            for (auto i = 0; i < h; i++) {
                auto xindex = x;
                for (auto j = 0; j < w; j++) {
                    auto sindex = ((grabber->width() * yindex) + xindex) * 4;
                    auto dindex = ((w * i) + j) * 4;
                    outImage.data[dindex + 2] = img[sindex + 0]; // B
                    outImage.data[dindex + 1] = img[sindex + 1]; // G
                    outImage.data[dindex + 0] = img[sindex + 2]; // R
                    outImage.data[dindex + 3] = img[sindex + 3]; // A
                    xindex++;
                }
                yindex++;
            }

            return _output;
        } else {
            throw ActivationError("Failed to grab screen");
        }
    }
};

// =============================================================================
// Input Simulation Shards
// =============================================================================

struct WaitKeyEvent : public WaitKeyEventBase {
    CFMachPortRef _eventTap;
    CFRunLoopSourceRef _runLoopSource;
    std::vector<SHVar> _events;
    size_t _maxQueueSize = 100;

    PARAM_VAR(_maxQueueSizeParam, "MaxQueueSize", "Maximum number of events to queue before dropping old ones.", {CoreInfo::IntType});
    PARAM_IMPL(PARAM_IMPL_FOR(_maxQueueSizeParam));

    void warmup(SHContext *context) {
        PARAM_WARMUP(context);
        if (_maxQueueSizeParam.valueType == SHType::Int) {
            _maxQueueSize = std::max((size_t)1, (size_t)_maxQueueSizeParam.payload.intValue);
        }
    }

    void cleanup(SHContext *context) {
        PARAM_CLEANUP(context);
        if (_eventTap) {
            CGEventTapEnable(_eventTap, false);
            CFMachPortInvalidate(_eventTap);
            CFRelease(_eventTap);
            _eventTap = nullptr;
        }
        if (_runLoopSource) {
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), _runLoopSource, kCFRunLoopCommonModes);
            CFRelease(_runLoopSource);
            _runLoopSource = nullptr;
        }
        _events.clear();
    }

    static CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
        WaitKeyEvent *self = (WaitKeyEvent *)refcon;

        if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
            int64_t keyCode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
            int state = (type == kCGEventKeyDown) ? 0 : 1;

            SHVar keyEvent = Var(state, (int)keyCode);

            if (self->_events.size() >= self->_maxQueueSize) {
                // Drop oldest events
                self->_events.erase(self->_events.begin(), self->_events.begin() + self->_maxQueueSize / 2);
            }

            self->_events.push_back(keyEvent);
        }

        return event;
    }

    SHVar activate(SHContext *context, const SHVar &input) {
        if (!_eventTap) {
            // Create event tap
            CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);
            _eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                                         kCGEventTapOptionDefault, eventMask,
                                         eventTapCallback, this);

            if (!_eventTap) {
                throw ActivationError("Failed to create event tap. Make sure accessibility permissions are granted.");
            }

            _runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, _eventTap, 0);
            CFRunLoopAddSource(CFRunLoopGetCurrent(), _runLoopSource, kCFRunLoopCommonModes);
            CGEventTapEnable(_eventTap, true);
        }

        // Wait for events
        while (_events.empty()) {
            SH_SUSPEND(context, 0.01);
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
        }

        SHVar event = _events.front();
        _events.erase(_events.begin());
        return event;
    }
};

struct SendKeyEvent : public SendKeyEventBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        int state = input.payload.int2Value[0];
        CGKeyCode keyCode = input.payload.int2Value[1];

        auto &windowVar = _window.get();

        if (windowVar.valueType != SHType::None) {
            auto window = AsAXWindow(windowVar);
            if (!window) {
                throw ActivationError("Window parameter is not a valid Desktop window!");
            }

            // For window-specific input, we need to post events to the specific process
            pid_t pid;
            AXError error = AXUIElementGetPid(window, &pid);
            if (error != kAXErrorSuccess) {
                throw ActivationError("Failed to get window PID");
            }

            // Create keyboard event targeted at process
            CGEventRef event = CGEventCreateKeyboardEvent(nullptr, keyCode, state == 0);
            if (event) {
                // Post to specific process using pid
                CGEventPostToPid(pid, event);
                CFRelease(event);
            }
        } else {
            // Global keyboard event
            CGEventRef event = CGEventCreateKeyboardEvent(nullptr, keyCode, state == 0);
            if (event) {
                CGEventPost(kCGHIDEventTap, event);
                CFRelease(event);
            }
        }

        return input;
    }
};

struct GetMousePos : public MousePosBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint location = CGEventGetLocation(event);
        CFRelease(event);

        auto &windowVar = _window.get();
        if (windowVar.valueType != SHType::None) {
            auto window = AsAXWindow(windowVar);
            if (window) {
                // Convert to window coordinates
                AXValueRef posValue = nullptr;
                AXError error = AXUIElementCopyAttributeValue(window, kAXPositionAttribute, (CFTypeRef *)&posValue);

                if (error == kAXErrorSuccess && posValue) {
                    CGPoint windowPos;
                    AXValueGetValue(posValue, (AXValueType)kAXValueCGPointType, &windowPos);
                    CFRelease(posValue);

                    location.x -= windowPos.x;
                    location.y -= windowPos.y;
                }
            }
        }

        return Var((int64_t)location.x, (int64_t)location.y);
    }
};

struct SetMousePos : public MousePosBase {
    static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }

    SHVar activate(SHContext *context, const SHVar &input) {
        CGPoint location = CGPointMake(input.payload.int2Value[0], input.payload.int2Value[1]);

        auto &windowVar = _window.get();
        if (windowVar.valueType != SHType::None) {
            auto window = AsAXWindow(windowVar);
            if (window) {
                // Convert from window coordinates
                AXValueRef posValue = nullptr;
                AXError error = AXUIElementCopyAttributeValue(window, kAXPositionAttribute, (CFTypeRef *)&posValue);

                if (error == kAXErrorSuccess && posValue) {
                    CGPoint windowPos;
                    AXValueGetValue(posValue, (AXValueType)kAXValueCGPointType, &windowPos);
                    CFRelease(posValue);

                    location.x += windowPos.x;
                    location.y += windowPos.y;
                }
            }
        }

        CGWarpMouseCursorPosition(location);
        return input;
    }
};

struct SetMouseRelativePos : public MousePosBase {
    static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }

    SHVar activate(SHContext *context, const SHVar &input) {
        int dx = input.payload.int2Value[0];
        int dy = input.payload.int2Value[1];

        // Get current position
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint location = CGEventGetLocation(event);
        CFRelease(event);

        // Move relative
        location.x += dx;
        location.y += dy;

        CGWarpMouseCursorPosition(location);
        return input;
    }
};

template <CGMouseButton BUTTON, CGEventType DOWN_TYPE, CGEventType UP_TYPE>
struct Click : public MousePosBase {
    bool _delays = true;

    PARAM_VAR(_delaysParam, "Natural", "Small pauses will be injected after click events.", {CoreInfo::BoolType});
    PARAM_IMPL(PARAM_IMPL_FOR(_delaysParam));

    void warmup(SHContext *context) {
        MousePosBase::warmup(context);
        PARAM_WARMUP(context);
        if (_delaysParam.valueType == SHType::Bool) {
            _delays = _delaysParam.payload.boolValue;
        }
    }

    void cleanup(SHContext *context) {
        MousePosBase::cleanup(context);
        PARAM_CLEANUP(context);
    }

    static SHTypesInfo inputTypes() { return CoreInfo::Int2Type; }

    SHVar activate(SHContext *context, const SHVar &input) {
        CGPoint location = CGPointMake(input.payload.int2Value[0], input.payload.int2Value[1]);

        auto &windowVar = _window.get();
        if (windowVar.valueType != SHType::None) {
            auto window = AsAXWindow(windowVar);
            if (window) {
                // Convert from window coordinates
                AXValueRef posValue = nullptr;
                AXError error = AXUIElementCopyAttributeValue(window, kAXPositionAttribute, (CFTypeRef *)&posValue);

                if (error == kAXErrorSuccess && posValue) {
                    CGPoint windowPos;
                    AXValueGetValue(posValue, (AXValueType)kAXValueCGPointType, &windowPos);
                    CFRelease(posValue);

                    location.x += windowPos.x;
                    location.y += windowPos.y;
                }
            }
        }

        // Mouse down
        CGEventRef downEvent = CGEventCreateMouseEvent(nullptr, DOWN_TYPE, location, BUTTON);
        CGEventPost(kCGHIDEventTap, downEvent);
        CFRelease(downEvent);

        if (_delays) {
            SH_SUSPEND(context, 0.05);
        }

        // Mouse up
        CGEventRef upEvent = CGEventCreateMouseEvent(nullptr, UP_TYPE, location, BUTTON);
        CGEventPost(kCGHIDEventTap, upEvent);
        CFRelease(upEvent);

        if (_delays) {
            SH_SUSPEND(context, 0.05);
        }

        return input;
    }
};

typedef Click<kCGMouseButtonLeft, kCGEventLeftMouseDown, kCGEventLeftMouseUp> LeftClick;
typedef Click<kCGMouseButtonRight, kCGEventRightMouseDown, kCGEventRightMouseUp> RightClick;
typedef Click<kCGMouseButtonCenter, kCGEventOtherMouseDown, kCGEventOtherMouseUp> MiddleClick;

template <bool HORIZONTAL>
struct Scroll : public MousePosBase {
    static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
    static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

    SHVar activate(SHContext *context, const SHVar &input) {
        float scrollAmount = input.payload.floatValue;

        CGEventRef event;
        if constexpr (HORIZONTAL) {
            event = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 2, 0, (int)scrollAmount);
        } else {
            event = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 1, (int)scrollAmount);
        }

        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);

        return input;
    }
};

struct ScrollHorizontal : public Scroll<true> {};
struct ScrollVertical : public Scroll<false> {};

struct LastInput : public LastInputBase {
    SHVar activate(SHContext *context, const SHVar &input) {
        double seconds = CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGAnyInputEventType);
        return Var(seconds);
    }
};

struct MouseHook : public MousePosBase {
    CFMachPortRef _eventTap;
    CFRunLoopSourceRef _runLoopSource;
    std::vector<SHVar> _events;
    size_t _maxQueueSize = 100;

    PARAM_VAR(_maxQueueSizeParam, "MaxQueueSize", "Maximum number of events to queue before dropping old ones.", {CoreInfo::IntType});
    PARAM_IMPL(PARAM_IMPL_FOR(_maxQueueSizeParam));

    static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
    static SHTypesInfo outputTypes() { return CoreInfo::Int4Type; }

    void warmup(SHContext *context) {
        MousePosBase::warmup(context);
        PARAM_WARMUP(context);
        if (_maxQueueSizeParam.valueType == SHType::Int) {
            _maxQueueSize = std::max((size_t)1, (size_t)_maxQueueSizeParam.payload.intValue);
        }
    }

    void cleanup(SHContext *context) {
        MousePosBase::cleanup(context);
        PARAM_CLEANUP(context);
        if (_eventTap) {
            CGEventTapEnable(_eventTap, false);
            CFMachPortInvalidate(_eventTap);
            CFRelease(_eventTap);
            _eventTap = nullptr;
        }
        if (_runLoopSource) {
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), _runLoopSource, kCFRunLoopCommonModes);
            CFRelease(_runLoopSource);
            _runLoopSource = nullptr;
        }
        _events.clear();
    }

    static CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
        MouseHook *self = (MouseHook *)refcon;

        CGPoint location = CGEventGetLocation(event);
        int state = -1;
        int button = -1;

        switch (type) {
            case kCGEventLeftMouseDown:
                state = 0; button = 0;
                break;
            case kCGEventLeftMouseUp:
                state = 1; button = 0;
                break;
            case kCGEventRightMouseDown:
                state = 0; button = 1;
                break;
            case kCGEventRightMouseUp:
                state = 1; button = 1;
                break;
            case kCGEventOtherMouseDown:
                state = 0; button = 2;
                break;
            case kCGEventOtherMouseUp:
                state = 1; button = 2;
                break;
            case kCGEventMouseMoved:
                state = -1; button = -1;
                break;
            default:
                return event;
        }

        SHVar mouseEvent{};
        mouseEvent.valueType = SHType::Int4;
        mouseEvent.payload.int4Value[0] = state;
        mouseEvent.payload.int4Value[1] = button;
        mouseEvent.payload.int4Value[2] = (int)location.x;
        mouseEvent.payload.int4Value[3] = (int)location.y;

        if (self->_events.size() >= self->_maxQueueSize) {
            self->_events.erase(self->_events.begin(), self->_events.begin() + self->_maxQueueSize / 2);
        }

        self->_events.push_back(mouseEvent);
        return event;
    }

    SHVar activate(SHContext *context, const SHVar &input) {
        if (!_eventTap) {
            CGEventMask eventMask = CGEventMaskBit(kCGEventLeftMouseDown) |
                                   CGEventMaskBit(kCGEventLeftMouseUp) |
                                   CGEventMaskBit(kCGEventRightMouseDown) |
                                   CGEventMaskBit(kCGEventRightMouseUp) |
                                   CGEventMaskBit(kCGEventOtherMouseDown) |
                                   CGEventMaskBit(kCGEventOtherMouseUp) |
                                   CGEventMaskBit(kCGEventMouseMoved);

            _eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                                         kCGEventTapOptionDefault, eventMask,
                                         eventTapCallback, this);

            if (!_eventTap) {
                throw ActivationError("Failed to create event tap. Make sure accessibility permissions are granted.");
            }

            _runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, _eventTap, 0);
            CFRunLoopAddSource(CFRunLoopGetCurrent(), _runLoopSource, kCFRunLoopCommonModes);
            CGEventTapEnable(_eventTap, true);
        }

        // Wait for events
        while (_events.empty()) {
            SH_SUSPEND(context, 0.01);
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
        }

        SHVar event = _events.front();
        _events.erase(_events.begin());
        return event;
    }
};

}; // namespace Desktop

SHARDS_REGISTER_FN(desktop) {
    using namespace Desktop;

    REGISTER_SHARD("Desktop.HasWindow", HasWindow);
    REGISTER_SHARD("Desktop.WaitWindow", WaitWindow);
    REGISTER_SHARD("Desktop.PID", PID);
    REGISTER_SHARD("Desktop.IsForeground", IsForeground);
    REGISTER_SHARD("Desktop.SetForeground", SetForeground);
    REGISTER_SHARD("Desktop.NotForeground", NotForeground);
    REGISTER_SHARD("Desktop.Resize", Resize);
    REGISTER_SHARD("Desktop.Move", Move);
    REGISTER_SHARD("Desktop.Size", WindowSize);
    REGISTER_SHARD("Desktop.Bounds", Bounds);
    REGISTER_SHARD("Desktop.SetTitle", SetTitle);

    REGISTER_SHARD("Desktop.Pixel", Pixel);
    REGISTER_SHARD("Desktop.Pixels", Pixels);
    REGISTER_SHARD("Desktop.CaptureFrame", CaptureFrame);

    REGISTER_SHARD("Desktop.WaitKeyEvent", WaitKeyEvent);
    REGISTER_SHARD("Desktop.SendKeyEvent", SendKeyEvent);
    REGISTER_SHARD("Desktop.GetMousePos", GetMousePos);
    REGISTER_SHARD("Desktop.SetMousePos", SetMousePos);
    REGISTER_SHARD("Desktop.MoveMouse", SetMouseRelativePos);
    REGISTER_SHARD("Desktop.LeftClick", LeftClick);
    REGISTER_SHARD("Desktop.RightClick", RightClick);
    REGISTER_SHARD("Desktop.MiddleClick", MiddleClick);
    REGISTER_SHARD("Desktop.ScrollHorizontal", ScrollHorizontal);
    REGISTER_SHARD("Desktop.ScrollVertical", ScrollVertical);
    REGISTER_SHARD("Desktop.LastInput", LastInput);
    REGISTER_SHARD("Desktop.MouseHook", MouseHook);
}