if(WIN32)
  set(SOURCES
    desktop.win.cpp
  )

  add_shards_module(desktop SOURCES ${SOURCES}
    REGISTER_SHARDS desktop
    EXPERIMENTAL)

  target_link_libraries(shards-module-desktop DXGI D3D11 ntdll)
elseif(DESKTOP_LINUX AND NOT ZIG_MUSL)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GIO REQUIRED gio-2.0 gio-unix-2.0)
  pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
  pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
  pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)

  add_shards_module(desktop SOURCES
    desktop.linux.cpp
    protocols/virtual-keyboard-unstable-v1-protocol.c
    protocols/wlr-virtual-pointer-unstable-v1-protocol.c
    REGISTER_SHARDS desktop
    EXPERIMENTAL)

  target_include_directories(shards-module-desktop PRIVATE
    ${GIO_INCLUDE_DIRS}
    ${PIPEWIRE_INCLUDE_DIRS}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/protocols)
  target_link_libraries(shards-module-desktop
    ${GIO_LIBRARIES}
    ${PIPEWIRE_LIBRARIES}
    ${WAYLAND_CLIENT_LIBRARIES}
    ${XKBCOMMON_LIBRARIES})
endif()
