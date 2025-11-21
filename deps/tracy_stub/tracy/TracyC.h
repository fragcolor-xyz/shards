// Tracy C API stub - provides empty macros when TRACY_ENABLE is OFF
#ifndef TRACY_C_H_STUB
#define TRACY_C_H_STUB

#ifdef __cplusplus
extern "C" {
#endif

// Empty macro stubs for C API
#define TracyCZone(c, x)
#define TracyCZoneN(c, x, y)
#define TracyCZoneC(c, x, y)
#define TracyCZoneNC(c, x, y, z)
#define TracyCZoneEnd(c)
#define TracyCFrameMark
#define TracyCFrameMarkNamed(x)
#define TracyCFrameMarkStart(x)
#define TracyCFrameMarkEnd(x)
#define TracyCAlloc(x, y)
#define TracyCFree(x)
#define TracyCAllocN(x, y, z)
#define TracyCFreeN(x, y)
#define TracyCMessage(x, y)
#define TracyCMessageL(x)
#define TracyCMessageC(x, y, z)
#define TracyCMessageLC(x, y)

#ifdef __cplusplus
}
#endif

#endif // TRACY_C_H_STUB
