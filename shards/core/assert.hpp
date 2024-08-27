#ifdef shassert
#undef shassert
#endif

#ifdef SH_RELWITHDEBINFO
#include <cstdlib>
#include <spdlog/spdlog.h>
#define shassert(_expr_)                              \
  if (!(_expr_)) {                                    \
    SPDLOG_CRITICAL("Assertion failed: {}", #_expr_); \
    std::abort();                                     \
  }

#define shassert_extended(_ctx_, _expr_)                                                        \
  if (!(_expr_)) {                                                                              \
    SPDLOG_CRITICAL("Assertion failed: {} in wire: {}", #_expr_, (_ctx_)->currentWire()->name); \
    std::abort();                                                                               \
  }
#else
#include <cassert>
#define shassert(_x) assert(_x)
#define shassert_extended(_ctx_, _x) assert(_x)
#endif
