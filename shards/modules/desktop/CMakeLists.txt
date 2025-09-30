if(WIN32)
  set(SOURCES
    desktop.win.cpp
  )

  add_shards_module(desktop SOURCES ${SOURCES}
    REGISTER_SHARDS desktop
    EXPERIMENTAL)

  target_link_libraries(shards-module-desktop DXGI D3D11 ntdll)

elseif(APPLE)
  set(SOURCES
    desktop.mac.mm
  )

  add_shards_module(desktop SOURCES ${SOURCES}
    REGISTER_SHARDS desktop
    EXPERIMENTAL)

  target_link_libraries(shards-module-desktop
    "-framework Cocoa"
    "-framework CoreGraphics"
    "-framework ApplicationServices"
    "-framework ScreenCaptureKit"
    "-framework Metal"
    "-framework CoreVideo"
    "-framework IOSurface"
  )

  # Require macOS 12.3+ for ScreenCaptureKit
  set_target_properties(shards-module-desktop PROPERTIES
    MACOSX_DEPLOYMENT_TARGET "12.3"
  )

  # Ensure Objective-C++ is used
  set_source_files_properties(desktop.mac.mm PROPERTIES
    COMPILE_FLAGS "-x objective-c++ -fobjc-arc"
  )

endif()
