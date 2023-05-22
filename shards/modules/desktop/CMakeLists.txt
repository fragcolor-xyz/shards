if(WIN32)
  set(SOURCES
    desktop.win.cpp
  )

  add_shards_module(desktop SOURCES ${SOURCES}
    REGISTER_SHARDS desktop
    EXPERIMENTAL)

  target_link_libraries(shards-module-desktop DXGI D3D11 ntdll)
endif()
