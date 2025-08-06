#ifndef F1FD68DC_779E_4167_A1ED_A1E172EDC4F6
#define F1FD68DC_779E_4167_A1ED_A1E172EDC4F6

#include <string>
#include <shards/core/platform.hpp>
#if SH_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif
#if SH_APPLE
#include <sys/sysctl.h>
#endif
#if SH_LINUX
#include <unistd.h>
#endif

namespace shards::dbg {
inline std::string getCmdLine() {
  std::string result;

#if SH_WINDOWS
  // Windows implementation
  LPWSTR *szArglist;
  int nArgs;

  szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
  if (szArglist != NULL) {
    for (int i = 0; i < nArgs; i++) {
      // Convert wide string to UTF-8
      int size_needed = WideCharToMultiByte(CP_UTF8, 0, szArglist[i], -1, NULL, 0, NULL, NULL);
      std::string arg(size_needed, 0);
      WideCharToMultiByte(CP_UTF8, 0, szArglist[i], -1, &arg[0], size_needed, NULL, NULL);

      // Remove null terminator that WideCharToMultiByte adds
      arg.pop_back();

      if (i > 0)
        result += " ";
      result += arg;
    }
    LocalFree(szArglist);
  }
#elif SH_APPLE || SH_LINUX
  // macOS and Linux implementation
  std::ifstream cmdline("/proc/self/cmdline");
  if (cmdline.is_open()) {
    std::string arg;
    bool first = true;
    while (std::getline(cmdline, arg, '\0')) {
      if (!first)
        result += " ";
      result += arg;
      first = false;
    }
  } else {
// Fallback for macOS which doesn't have /proc
#if SH_APPLE
    int mib[4] = {CTL_KERN, KERN_PROCARGS2, getpid(), 0};
    size_t size = 0;

    if (sysctl(mib, 3, NULL, &size, NULL, 0) == 0) {
      std::vector<char> buffer(size);
      if (sysctl(mib, 3, buffer.data(), &size, NULL, 0) == 0) {
        // Skip past argc
        int argc = *reinterpret_cast<int *>(buffer.data());
        char *start = buffer.data() + sizeof(int);

        // Skip past exec_path
        while (*start != '\0')
          start++;
        start++; // Skip the terminating null

        // Skip any additional nulls
        while (*start == '\0')
          start++;

        // Now we're at the beginning of argv
        for (int i = 0; i < argc; i++) {
          if (i > 0)
            result += " ";
          result += start;
          start += strlen(start) + 1;
        }
      }
    }
#endif
  }
#else
  // Fallback for other platforms
  result = "unknown";
#endif

  return result;
}
} // namespace shards::dbg

#endif /* F1FD68DC_779E_4167_A1ED_A1E172EDC4F6 */
