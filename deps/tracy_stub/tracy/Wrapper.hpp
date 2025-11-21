// Tracy stub - provides empty macros when TRACY_ENABLE is OFF
#ifndef TRACY_WRAPPER_HPP_STUB
#define TRACY_WRAPPER_HPP_STUB

// Empty macro stubs
#define ZoneScoped
#define ZoneScopedN(x)
#define ZoneScopedC(x)
#define ZoneScopedNC(x, y)
#define ZoneText(x, y)
#define ZoneName(x, y)
#define ZoneValue(x)
#define FrameMark
#define FrameMarkNamed(x)
#define FrameMarkStart(x)
#define FrameMarkEnd(x)
#define TracyLockable(type, varname) type varname
#define TracyLockableN(type, varname, desc) type varname
#define TracySharedLockable(type, varname) type varname
#define TracySharedLockableN(type, varname, desc) type varname
#define LockableBase(type) type
#define SharedLockableBase(type) type
#define LockMark(x) (void)x
#define LockableName(x, y, z)
#define TracyPlot(x, y)
#define TracyMessage(x, y)
#define TracyMessageL(x)
#define TracyMessageC(x, y, z)
#define TracyMessageLC(x, y)
#define TracyAlloc(x, y)
#define TracyFree(x)
#define TracyAllocN(x, y, z)
#define TracyFreeN(x, y)
#define TracyFiberEnter(x)
#define TracyFiberLeave
#define ZoneTransient(x, y)
#define ZoneTransientN(x, y, z)

namespace tracy {
    // Stub types
    struct SourceLocationData {
        const char* name;
        const char* function;
        const char* file;
        uint32_t line;
        uint32_t color;
    };
}

#endif // TRACY_WRAPPER_HPP_STUB
