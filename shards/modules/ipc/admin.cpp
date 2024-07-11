#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

#ifdef _WIN32
#define SHAlloc SHAlloc1
#define SHFree SHFree1
#include <windows.h>
#include <ShlObj.h>
#undef SHAlloc
#undef SHFree
#endif

struct RunElevated {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM(shards::ShardsVar, _contents, "Contents", "Run contents when elevated, or restart current program if not",
        {shards::CoreInfo::ShardRefSeqType});
  PARAM(shards::ShardsVar, _contents, "Args", "Run contents when elevator, or restart current program if not",
        {shards::CoreInfo::ShardRefSeqType});
  PARAM_IMPL(PARAM_IMPL_FOR(_contents));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup() { PARAM_CLEANUP(); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _contents.compose(data);
    for (auto &required : _contents.composeResult().requiredInfo) {
      _requiredVariables.push_back(required);
    }
    return shards::CoreInfo::NoneType;
  }

  SHVar activateInner(SHContext *shContext, const SHVar &input) {
    SHVar output{};
    _contents.activate(shContext, input, output);
    return output;
  }

#ifdef _WIN32
  bool isRunningElevated() {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token))
      throw std::runtime_error("Failed to open process token");

    try {
#if 0 && _WIN32_WINNT >= 0x0600
      bool isAdmin = false;
      DWORD bytesUsed = 0;
      TOKEN_ELEVATION_TYPE tokenElevationType;

      if (!::GetTokenInformation(token, TokenElevationType, &tokenElevationType, sizeof(tokenElevationType), &bytesUsed)) {
        const DWORD lastError = ::GetLastError();

        throw std::runtime_error("GetTokenInformation - TokenElevationType");
      }

      if (tokenElevationType == TokenElevationTypeLimited) {
        HANDLE unfilteredToken;

        if (!::GetTokenInformation(token, TokenLinkedToken, &unfilteredToken, sizeof(HANDLE), &bytesUsed)) {
          const DWORD lastError = ::GetLastError();

          throw std::runtime_error("GetTokenInformation - TokenLinkedToken");
        }

        BYTE adminSID[SECURITY_MAX_SID_SIZE];

        DWORD sidSize = sizeof(adminSID);

        if (!::CreateWellKnownSid(WinBuiltinAdministratorsSid, 0, &adminSID, &sidSize)) {
          const DWORD lastError = ::GetLastError();

          throw std::runtime_error("CreateWellKnownSid");
        }

        BOOL isMember = FALSE;

        if (::CheckTokenMembership(unfilteredToken, &adminSID, &isMember) == 0) {
          const DWORD lastError = ::GetLastError();

          throw std::runtime_error("CheckTokenMembership");
        }

        isAdmin = (isMember != FALSE);
      } else {
        isAdmin = ::IsUserAnAdmin();
      }

      return isAdmin;

#else
      return ::IsUserAnAdmin();
#endif
    } catch (...) {
      CloseHandle(token);
      throw;
    }
  }

  SHVar runElevated(SHContext *shContext, const SHVar &input) {
    if (isRunningElevated()) {
      return activateInner(shContext, input);
    } else {
      // ShellExecuteA(NULL, "runas", shards::GetGlobals().ExePath.c_str(), GetCommandLineA(),
      //               shards::GetGlobals().RootPath.c_str(), // default dir
      //               SW_SHOWNORMAL);

      std::string exePath;
      exePath.resize(1024);
      exePath.resize(GetModuleFileNameA(nullptr, exePath.data(), exePath.size()));

      SHELLEXECUTEINFOA shExInfo = {0};
      shExInfo.cbSize = sizeof(shExInfo);
      shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
      shExInfo.hwnd = 0;
      shExInfo.lpVerb = ("runas");               // Operation to perform
      shExInfo.lpFile = exePath.c_str();         // Application to start
      shExInfo.lpParameters = GetCommandLineA(); // Additional parameters
      shExInfo.lpDirectory = 0;
      shExInfo.nShow = SW_SHOW;
      shExInfo.hInstApp = 0;

      if (ShellExecuteExA(&shExInfo)) {
        WaitForSingleObject(shExInfo.hProcess, INFINITE);
        CloseHandle(shExInfo.hProcess);
      }
      ExitProcess(0);
    }
  }
#else
  SHVar runElevated(SHContext *shContext, const SHVar &input) { return activateInner(shContext, input); }
#endif

  SHVar activate(SHContext *shContext, const SHVar &input) { return runElevated(shContext, input); }
};

SHARDS_REGISTER_FN(admin) { REGISTER_SHARD("RunElevated", RunElevated); }
