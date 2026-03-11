/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SH_DESKTOP_PORTAL_LINUX
#define SH_DESKTOP_PORTAL_LINUX

#include <gio/gio.h>
#include <shards/log/log.hpp>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>

namespace Desktop {

static inline shards::logging::Logger getLogger() {
  static auto logger = shards::logging::getOrCreate("Desktop.Portal");
  return logger;
}

// Manages an xdg-desktop-portal ScreenCast session (with optional RemoteDesktop upgrade).
// On compositors that support RemoteDesktop (GNOME, KDE), uses that interface for the session.
// On compositors with ScreenCast only (Hyprland, wlroots), falls back automatically.
// Input injection is handled separately via UInputDevice (/dev/uinput).
class PortalSession {
public:
  enum class State { Idle, Pending, Active, Failed };

  PortalSession() = default;
  ~PortalSession() { destroy(); }

  PortalSession(const PortalSession &) = delete;
  PortalSession &operator=(const PortalSession &) = delete;

  // Initiates the portal session (async). Call poll() until state is Active or Failed.
  bool start() {
    if (_state != State::Idle)
      return false;

    _state = State::Pending;

    _connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!_connection) {
      SPDLOG_LOGGER_ERROR(getLogger(), "Failed to connect to session bus");
      _state = State::Failed;
      return false;
    }

    // Generate unique handle token
    _handleToken = "shards_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    _sessionToken = "shards_session_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    // Step 1: Try RemoteDesktop.CreateSession first, fall back to ScreenCast
    createSession();
    return true;
  }

  // Pump the GLib main context to process D-Bus callbacks.
  // Must be called periodically while state is Pending.
  void poll() {
    GMainContext *ctx = g_main_context_default();
    while (g_main_context_iteration(ctx, FALSE)) {
      // drain all pending events
    }

    // Timeout for RemoteDesktop.CreateSession — on compositors like Hyprland that
    // don't support RemoteDesktop, the D-Bus call never sends a Response signal.
    // After 3 seconds, fall back to ScreenCast.
    if (_waitingForCreateSession && _useRemoteDesktop) {
      auto elapsed = std::chrono::steady_clock::now() - _createSessionTime;
      if (elapsed > std::chrono::seconds(3)) {
        SPDLOG_LOGGER_INFO(getLogger(), "RemoteDesktop.CreateSession timed out after 3s — falling back to ScreenCast");
        _waitingForCreateSession = false;
        _useRemoteDesktop = false;
        createSessionScreenCast();
      }
    }
  }

  // Returns current state
  State state() const { return _state; }
  bool isActive() const { return _state == State::Active; }

  // Whether RemoteDesktop interface was used (vs ScreenCast-only)
  bool isRemoteDesktop() const { return _useRemoteDesktop; }

  // Cleanup everything
  void destroy() {
    if (_responseSubscription > 0) {
      g_dbus_connection_signal_unsubscribe(_connection, _responseSubscription);
      _responseSubscription = 0;
    }

    if (_sessionPath.size() > 0 && _connection) {
      // Close the session via D-Bus
      g_dbus_connection_call_sync(_connection, "org.freedesktop.portal.Desktop", _sessionPath.c_str(),
                                  "org.freedesktop.portal.Session", "Close", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, -1,
                                  nullptr, nullptr);
      _sessionPath.clear();
    }

    if (_connection) {
      g_object_unref(_connection);
      _connection = nullptr;
    }

    _state = State::Idle;
    _pipewireFd = -1;
    _pipewireNode = 0;
  }

  int pipewireFd() const { return _pipewireFd; }
  uint32_t pipewireNode() const { return _pipewireNode; }

private:
  void createSession() {
    _useRemoteDesktop = true;

    GVariantBuilder optionsBuilder;
    g_variant_builder_init(&optionsBuilder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&optionsBuilder, "{sv}", "handle_token", g_variant_new_string(_handleToken.c_str()));
    g_variant_builder_add(&optionsBuilder, "{sv}", "session_handle_token", g_variant_new_string(_sessionToken.c_str()));

    // Subscribe for the response signal before making the call
    subscribeResponse([this](uint32_t response, GVariant *results) {
      _waitingForCreateSession = false;

      if (response != 0) {
        if (_useRemoteDesktop) {
          // RemoteDesktop.CreateSession failed — fall back to ScreenCast
          SPDLOG_LOGGER_INFO(getLogger(), "RemoteDesktop.CreateSession returned {} — falling back to ScreenCast", response);
          _useRemoteDesktop = false;
          createSessionScreenCast();
          return;
        }
        SPDLOG_LOGGER_ERROR(getLogger(), "CreateSession failed with response {}", response);
        _state = State::Failed;
        return;
      }

      // Extract session handle
      const char *sessionHandle = nullptr;
      g_variant_lookup(results, "session_handle", "&s", &sessionHandle);
      if (sessionHandle) {
        _sessionPath = sessionHandle;
        SPDLOG_LOGGER_INFO(getLogger(), "Session created (RemoteDesktop): {}", _sessionPath);
        selectDevices();
      } else {
        SPDLOG_LOGGER_ERROR(getLogger(), "No session_handle in response");
        _state = State::Failed;
      }
    });

    SPDLOG_LOGGER_INFO(getLogger(), "Trying RemoteDesktop.CreateSession...");
    _waitingForCreateSession = true;
    _createSessionTime = std::chrono::steady_clock::now();
    g_dbus_connection_call(_connection, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                           "org.freedesktop.portal.RemoteDesktop", "CreateSession", g_variant_new("(a{sv})", &optionsBuilder),
                           nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
  }

  void createSessionScreenCast() {
    // Regenerate tokens for the new attempt
    _handleToken = "shards_sc_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    _sessionToken = "shards_sc_session_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    GVariantBuilder optionsBuilder;
    g_variant_builder_init(&optionsBuilder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&optionsBuilder, "{sv}", "handle_token", g_variant_new_string(_handleToken.c_str()));
    g_variant_builder_add(&optionsBuilder, "{sv}", "session_handle_token", g_variant_new_string(_sessionToken.c_str()));

    subscribeResponse([this](uint32_t response, GVariant *results) {
      if (response != 0) {
        SPDLOG_LOGGER_ERROR(getLogger(), "ScreenCast.CreateSession failed with response {}", response);
        _state = State::Failed;
        return;
      }

      const char *sessionHandle = nullptr;
      g_variant_lookup(results, "session_handle", "&s", &sessionHandle);
      if (sessionHandle) {
        _sessionPath = sessionHandle;
        SPDLOG_LOGGER_INFO(getLogger(), "Session created (ScreenCast): {}", _sessionPath);
        // ScreenCast path: skip selectDevices, go straight to selectSources
        selectSources();
      } else {
        SPDLOG_LOGGER_ERROR(getLogger(), "No session_handle in response");
        _state = State::Failed;
      }
    });

    SPDLOG_LOGGER_INFO(getLogger(), "Trying ScreenCast.CreateSession...");
    g_dbus_connection_call(_connection, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                           "org.freedesktop.portal.ScreenCast", "CreateSession", g_variant_new("(a{sv})", &optionsBuilder),
                           nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
  }

  void selectDevices() {
    _handleToken = "shards_dev_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    GVariantBuilder optionsBuilder;
    g_variant_builder_init(&optionsBuilder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&optionsBuilder, "{sv}", "handle_token", g_variant_new_string(_handleToken.c_str()));
    // Request keyboard (1) + pointer (2)
    g_variant_builder_add(&optionsBuilder, "{sv}", "types", g_variant_new_uint32(1 | 2));

    subscribeResponse([this](uint32_t response, GVariant *results) {
      if (response != 0) {
        SPDLOG_LOGGER_ERROR(getLogger(), "SelectDevices failed with response {}", response);
        _state = State::Failed;
        return;
      }
      SPDLOG_LOGGER_INFO(getLogger(), "Devices selected");
      selectSources();
    });

    g_dbus_connection_call(_connection, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                           "org.freedesktop.portal.RemoteDesktop", "SelectDevices",
                           g_variant_new("(oa{sv})", _sessionPath.c_str(), &optionsBuilder), nullptr, G_DBUS_CALL_FLAGS_NONE, -1,
                           nullptr, nullptr, nullptr);
  }

  void selectSources() {
    _handleToken = "shards_src_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    GVariantBuilder optionsBuilder;
    g_variant_builder_init(&optionsBuilder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&optionsBuilder, "{sv}", "handle_token", g_variant_new_string(_handleToken.c_str()));
    // Source types: Monitor (1) + Window (2)
    g_variant_builder_add(&optionsBuilder, "{sv}", "types", g_variant_new_uint32(1 | 2));
    // Allow multiple sources
    g_variant_builder_add(&optionsBuilder, "{sv}", "multiple", g_variant_new_boolean(FALSE));

    subscribeResponse([this](uint32_t response, GVariant *results) {
      if (response != 0) {
        SPDLOG_LOGGER_ERROR(getLogger(), "SelectSources failed with response {}", response);
        _state = State::Failed;
        return;
      }
      SPDLOG_LOGGER_INFO(getLogger(), "Sources selected");
      startSession();
    });

    g_dbus_connection_call(_connection, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                           "org.freedesktop.portal.ScreenCast", "SelectSources",
                           g_variant_new("(oa{sv})", _sessionPath.c_str(), &optionsBuilder), nullptr, G_DBUS_CALL_FLAGS_NONE, -1,
                           nullptr, nullptr, nullptr);
  }

  void startSession() {
    _handleToken = "shards_start_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    GVariantBuilder optionsBuilder;
    g_variant_builder_init(&optionsBuilder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&optionsBuilder, "{sv}", "handle_token", g_variant_new_string(_handleToken.c_str()));

    subscribeResponse([this](uint32_t response, GVariant *results) {
      if (response != 0) {
        SPDLOG_LOGGER_ERROR(getLogger(), "Start failed with response {} (user may have denied access)", response);
        _state = State::Failed;
        return;
      }

      // Extract PipeWire streams info
      GVariant *streams = nullptr;
      g_variant_lookup(results, "streams", "@a(ua{sv})", &streams);
      if (streams && g_variant_n_children(streams) > 0) {
        GVariant *firstStream = g_variant_get_child_value(streams, 0);
        g_variant_get_child(firstStream, 0, "u", &_pipewireNode);
        g_variant_unref(firstStream);
        SPDLOG_LOGGER_INFO(getLogger(), "PipeWire node: {}", _pipewireNode);
      }
      if (streams)
        g_variant_unref(streams);

      // Get PipeWire fd via OpenPipeWireRemote
      GError *error = nullptr;
      GUnixFDList *fdList = nullptr;
      GVariantBuilder emptyOptions;
      g_variant_builder_init(&emptyOptions, G_VARIANT_TYPE_VARDICT);

      GVariant *fdResult = g_dbus_connection_call_with_unix_fd_list_sync(
          _connection, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.ScreenCast",
          "OpenPipeWireRemote", g_variant_new("(oa{sv})", _sessionPath.c_str(), &emptyOptions), G_VARIANT_TYPE("(h)"),
          G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &fdList, nullptr, &error);

      if (error) {
        SPDLOG_LOGGER_ERROR(getLogger(), "OpenPipeWireRemote failed: {}", error->message);
        g_error_free(error);
        _state = State::Failed;
        return;
      }

      if (fdResult && fdList) {
        int32_t fdIndex = 0;
        g_variant_get(fdResult, "(h)", &fdIndex);
        _pipewireFd = g_unix_fd_list_get(fdList, fdIndex, nullptr);
        g_variant_unref(fdResult);
        g_object_unref(fdList);
        SPDLOG_LOGGER_INFO(getLogger(), "PipeWire fd: {}", _pipewireFd);
      }

      _state = State::Active;
      SPDLOG_LOGGER_INFO(getLogger(), "Portal session is now active (mode: {})", _useRemoteDesktop ? "RemoteDesktop" : "ScreenCast");
    });

    // Use the appropriate interface for Start
    const char *iface = _useRemoteDesktop ? "org.freedesktop.portal.RemoteDesktop" : "org.freedesktop.portal.ScreenCast";
    SPDLOG_LOGGER_INFO(getLogger(), "Calling {}.Start", iface);

    // Use parent_window "" for no parent
    g_dbus_connection_call(_connection, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", iface, "Start",
                           g_variant_new("(osa{sv})", _sessionPath.c_str(), "", &optionsBuilder), nullptr, G_DBUS_CALL_FLAGS_NONE,
                           -1, nullptr, nullptr, nullptr);
  }

  using ResponseCallback = std::function<void(uint32_t response, GVariant *results)>;

  void subscribeResponse(ResponseCallback callback) {
    // Unsubscribe previous if any
    if (_responseSubscription > 0) {
      g_dbus_connection_signal_unsubscribe(_connection, _responseSubscription);
      _responseSubscription = 0;
    }

    _responseCallback = std::move(callback);

    // Build the expected response object path
    const char *uniqueName = g_dbus_connection_get_unique_name(_connection);
    std::string senderName(uniqueName + 1); // skip leading ':'
    for (auto &c : senderName) {
      if (c == '.')
        c = '_';
    }
    std::string responsePath = "/org/freedesktop/portal/desktop/request/" + senderName + "/" + _handleToken;

    _responseSubscription = g_dbus_connection_signal_subscribe(
        _connection, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request", "Response", responsePath.c_str(),
        nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        [](GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *, GVariant *parameters,
           gpointer userData) {
          auto self = static_cast<PortalSession *>(userData);
          uint32_t response = 0;
          GVariant *results = nullptr;
          g_variant_get(parameters, "(u@a{sv})", &response, &results);
          if (self->_responseCallback) {
            self->_responseCallback(response, results);
          }
          if (results)
            g_variant_unref(results);
        },
        this, nullptr);
  }

  GDBusConnection *_connection = nullptr;
  std::string _sessionPath;
  std::string _handleToken;
  std::string _sessionToken;
  State _state = State::Idle;
  guint _responseSubscription = 0;
  ResponseCallback _responseCallback;
  bool _useRemoteDesktop = true;
  bool _waitingForCreateSession = false;
  std::chrono::steady_clock::time_point _createSessionTime;

  int _pipewireFd = -1;
  uint32_t _pipewireNode = 0;
};

} // namespace Desktop

#endif // SH_DESKTOP_PORTAL_LINUX
