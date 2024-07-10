#ifndef B2D0468B_F6FA_4CE8_A77E_096DE94923C2
#define B2D0468B_F6FA_4CE8_A77E_096DE94923C2

#if defined(_WIN32)
#define SH_WINDOWS 1
#elif defined(__ANDROID__)
#define SH_ANDROID 1
#elif defined(__EMSCRIPTEN__)
#define SH_EMSCRIPTEN 1
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#define SH_APPLE 1

#if TARGET_OS_IPHONE // includes all iOS-based devices including visionOS
#define SH_IOS 1
#if TARGET_OS_VISION // nested there is also vision pro
#define SH_VISION 1
#endif
#else
#define SH_OSX 1
#endif

#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
#define SH_LINUX 1
#else
#define SH_UNKNOWN 1
#endif

#ifndef SH_ANDROID
#define SH_ANDROID 0
#endif

#ifndef SH_APPLE
#define SH_APPLE 0
#endif

#ifndef SH_OSX
#define SH_OSX 0
#endif

#ifndef SH_IOS
#define SH_IOS 0
#endif

#ifndef SH_WINDOWS
#define SH_WINDOWS 0
#endif

#ifndef SH_LINUX
#define SH_LINUX 0
#endif

#ifndef SH_EMSCRIPTEN
#define SH_EMSCRIPTEN 0
#endif

#endif /* B2D0468B_F6FA_4CE8_A77E_096DE94923C2 */
