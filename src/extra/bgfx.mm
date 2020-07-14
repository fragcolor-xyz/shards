
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#ifdef SDL_VIDEO_DRIVER_UIKIT
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

namespace BGFX {
void *cbSetupMetalLayer(void *wnd) {
  @autoreleasepool {
#ifdef SDL_VIDEO_DRIVER_UIKIT
  UIWindow *window = (UIWindow*)wnd;
  UIView *contentView = [[window rootViewController] view];
  CAMetalLayer *res = [CAMetalLayer layer];
  [[contentView layer] addSublayer:res];
  return res;
#else // cocoa
  NSWindow *window = (NSWindow*)wnd;
  NSView *contentView = [window contentView];
  [contentView setWantsLayer:YES];
  CAMetalLayer *res = [CAMetalLayer layer];
  [contentView setLayer:res];
  return res;
#endif
  }
}
}
