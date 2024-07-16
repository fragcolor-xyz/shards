#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace shards;

struct RegisterSchemeHandler {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_scheme, "Scheme", "The name of the scheme, as in \"http\" in \"http://something\"",
                 {shards::CoreInfo::StringType});
  PARAM_PARAMVAR(_command, "Command", "Command to execute when the scheme is invoked (%1 indicates the argument)",
                 {shards::CoreInfo::StringType, shards::CoreInfo::StringVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_scheme), PARAM_IMPL_FOR(_command));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_scheme.isNone())
      throw std::runtime_error("Scheme is required");
    return shards::CoreInfo::NoneType;
  }

#ifdef _WIN32
  void registerHandler() {
    // HKEY_CURRENT_USER/Software/Classes/
    // your-protocol-name/
    //   (Default)    "URL:your-protocol-name Protocol"
    //   URL Protocol ""
    //   shell/
    //     open/
    //       command/
    //         (Default) PathToExecutable
    Var argsToSelfVar = (Var &)_command.get();
    std::string_view args;
    if (!argsToSelfVar.isNone()) {
      args = (std::string_view)argsToSelfVar;
    } else {
      args = " ";
    }

    std::vector<HKEY> openHandles;
    DEFER({
      for (auto &h : openHandles) {
        RegCloseKey(h);
      }
    });

    auto createKey = [&](HKEY parent, const char *path) {
      HKEY key{};
      HRESULT res = RegCreateKey(parent, path, &key);
      if (res != ERROR_SUCCESS) {
        throw std::runtime_error(fmt::format("Failed to create registry key ({})", res));
      }
      openHandles.push_back(key);
      return key;
    };

    auto setKey = [](HKEY key, std::optional<std::string> valueName, std::string arg) {
      HRESULT hr = RegSetKeyValueA(key, NULL, valueName ? valueName->c_str() : nullptr, REG_SZ, arg.data(), arg.size());
      if (hr != ERROR_SUCCESS)
        throw std::runtime_error(fmt::format("Failed to set registry key ({})", hr));
    };

    auto schemeName = (std::string_view)(Var &)_scheme.get();
    std::string baseKey = fmt::format("{}", schemeName);
    std::string commandValue = (std::string)(Var &)_command.get();

    HKEY k0 = createKey(createKey(createKey(HKEY_CURRENT_USER, "Software"), "Classes"), baseKey.c_str());
    setKey(k0, "URL Protocol", "");
    setKey(k0, std::nullopt, fmt::format("URL:{} Protocol", schemeName));
    HKEY k2 = createKey(createKey(createKey(k0, "shell"), "open"), "command");
    setKey(k2, std::nullopt, commandValue);
  }
#else
  void registerHandler() {}
#endif
  SHVar activate(SHContext *shContext, const SHVar &input) {
    registerHandler();
    return SHVar{};
  }
};

struct IsSchemeHandlerInstalled {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_scheme, "Scheme", "The name of the scheme, as in \"http\" in \"http://something\"",
                 {shards::CoreInfo::StringType});
  PARAM_PARAMVAR(_command, "Command", "Command to execute when the scheme is invoked (%1 indicates the argument)",
                 {shards::CoreInfo::StringType, shards::CoreInfo::StringVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_scheme), PARAM_IMPL_FOR(_command));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_scheme.isNone())
      throw std::runtime_error("Scheme is required");
    return shards::CoreInfo::NoneType;
  }

#ifdef _WIN32
  bool isInstalled() {
    std::vector<HKEY> openHandles;
    DEFER({
      for (auto &h : openHandles) {
        RegCloseKey(h);
      }
    });

    auto openKey = [&](HKEY parent, const char *path) {
      HKEY key{};
      HRESULT res = RegOpenKeyExA(parent, path, 0, KEY_READ, &key);
      if (res != ERROR_SUCCESS) {
        throw std::runtime_error(fmt::format("Failed to create registry key ({})", res));
      }
      openHandles.push_back(key);
      return key;
    };

    auto readKey = [&](HKEY key, const char *valueName) {
      std::string buf;
      buf.resize(1024);
      DWORD size = buf.size();
      if (RegGetValueA(key, NULL, valueName, RRF_RT_REG_SZ, NULL, buf.data(), &size) != ERROR_SUCCESS) {
        throw std::runtime_error(fmt::format("Failed to read registry key ({})", GetLastError()));
      }
      buf.resize(size - 1);
      return buf;
    };

    std::string baseKey = fmt::format("{}", (std::string_view)(Var &)_scheme.get());
    std::string commandValue = (std::string)(Var &)_command.get();
    auto schemeName = fmt::format("URL:{} Protocol", (std::string_view)(Var &)_scheme.get());

    try {
      HKEY k0 = openKey(openKey(openKey(HKEY_CURRENT_USER, "Software"), "Classes"), baseKey.c_str());
      HKEY k2 = openKey(openKey(openKey(k0, "shell"), "open"), "command");
      if (readKey(k0, nullptr) != schemeName)
        return false;
      if (readKey(k0, "URL Protocol") != "")
        return false;
      if (readKey(k2, NULL) != commandValue)
        return false;
    } catch (...) {
      return false;
    }
    return true;
  }
#else
  bool isInstalled() { return true; }
#endif
  SHVar activate(SHContext *shContext, const SHVar &input) { return Var(isInstalled()); }
};

SHARDS_REGISTER_FN(schemes) {
  REGISTER_SHARD("SchemeHandler.Register", RegisterSchemeHandler);
  REGISTER_SHARD("SchemeHandler.IsInstalled", IsSchemeHandlerInstalled);
}
