#ifdef __clang__
#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#endif

#ifdef __clang__
#pragma clang attribute pop
#endif

#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif

// #ifndef NDEBUG
// #define MA_LOG_LEVEL MA_LOG_LEVEL_WARNING
// #define MA_DEBUG_OUTPUT 1
// #endif

#include <miniaudio/extras/stb_vorbis.c>
#undef R
#undef L
#undef C
#include <miniaudio.c>

#ifdef __clang__
#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#endif

#ifdef __clang__
#pragma clang attribute pop
#endif