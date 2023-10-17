#ifndef F676CA55_B5BE_4F37_8AF8_84A2AD0989F5
#define F676CA55_B5BE_4F37_8AF8_84A2AD0989F5

#include <string>
#include <string_view>
#include <list>
#include <cassert>
#include <spdlog/fmt/fmt.h>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <pthread.h>
#endif

#define SH_DEBUG_THREAD_NAMES 1

namespace shards {

#ifdef _WIN32
inline std::wstring toWindowsWString(std::string_view utf8) {
  std::wstring result;
  result.resize(MultiByteToWideChar(CP_UTF8, 0, utf8.data(), utf8.size(), nullptr, 0));
  result.resize(MultiByteToWideChar(CP_UTF8, 0, utf8.data(), utf8.size(), result.data(), result.size()));
  return result;
}
#endif

inline void setThreadName(std::string_view name_sv) {
#ifdef _WIN32
  std::wstring name = toWindowsWString(name_sv);
  SetThreadDescription(GetCurrentThread(), name.c_str());
#endif
}

std::list<std::string> &getThreadNameStack();

inline void pushThreadName(std::string_view name) {
  auto &stack = getThreadNameStack();
  stack.emplace_back(name);
  setThreadName(name);
}

inline void popThreadName() {
  auto &stack = getThreadNameStack();
  assert(stack.size() > 0);
  // if (stack.size() > 0)
  stack.pop_back();
  setThreadName(stack.size() > 0 ? stack.back() : "Unnamed thread");
}
} // namespace shards

#endif /* F676CA55_B5BE_4F37_8AF8_84A2AD0989F5 */
