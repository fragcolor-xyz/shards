set(SOURCES demo.cpp)

if(ANDROID)
  add_library(virtual-ui-demo-lib SHARED ${SOURCES})
  set_target_properties(virtual-ui-demo-lib PROPERTIES OUTPUT_NAME "virtual-ui-demo")
  set(TARGET_NAME virtual-ui-demo-lib)

  include(Android)
  file(GLOB_RECURSE JAVA_SOURCES "android/src/*")
  file(GLOB_RECURSE SDL_JAVA_SOURCES "${SHARDS_DIR}/deps/SDL/android-project/app/src/main/java/*")
  add_android_package(
      NAME virtual-ui-demo
      SOURCES ${JAVA_SOURCES} ${SDL_JAVA_SOURCES}
      MANIFEST android/AndroidManifest.xml
      RES_PATH android/res
      LIB_TARGETS virtual-ui-demo-lib
  )
else()
  set(TARGET_NAME virtual-ui-demo)
  add_executable(virtual-ui-demo ${SOURCES})

  set_target_properties(
    virtual-ui-demo
    PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "Shards Virtual UI Demo"
    MACOSX_BUNDLE_BUNDLE_NAME "xyz.fragcolor.virtual-ui-demo"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
  )
endif()

target_link_libraries(${TARGET_NAME} gfx virtual-ui virtual-ui-test-elements input linalg spdlog)
