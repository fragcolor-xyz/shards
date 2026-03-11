if(WIN32)
  set(SOURCES
    desktop.win.cpp
  )

  add_shards_module(desktop SOURCES ${SOURCES}
    REGISTER_SHARDS desktop
    EXPERIMENTAL)

  target_link_libraries(shards-module-desktop DXGI D3D11 ntdll)
elseif(DESKTOP_LINUX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GIO REQUIRED gio-2.0 gio-unix-2.0)
  pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

  add_shards_module(desktop SOURCES desktop.linux.cpp
    REGISTER_SHARDS desktop
    EXPERIMENTAL)

  target_include_directories(shards-module-desktop PRIVATE
    ${GIO_INCLUDE_DIRS}
    ${PIPEWIRE_INCLUDE_DIRS})
  target_link_libraries(shards-module-desktop
    ${GIO_LIBRARIES}
    ${PIPEWIRE_LIBRARIES})
endif()
