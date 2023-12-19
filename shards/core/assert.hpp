#ifdef shassert
#undef shassert
#endif

#ifdef SH_RELWITHDEBINFO
#include <cstdlib>
#define shassert(_expr_)                              \
  if (!(_expr_)) {                                    \
    SPDLOG_CRITICAL("Assertion failed: {}", #_expr_); \
    std::abort();                                     \
  }
#else
#include <cassert>
#define shassert(_x) assert(_x)
#endif
