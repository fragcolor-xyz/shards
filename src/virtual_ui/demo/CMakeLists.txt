set(SOURCES demo.cpp)

if(ANDROID)
  add_library(virtual_ui_demo_lib SHARED ${SOURCES})
  set_target_properties(virtual_ui_demo_lib PROPERTIES OUTPUT_NAME "virtual_ui_demo")
  set(TARGET_NAME virtual_ui_demo_lib)

  include(Android)
  file(GLOB_RECURSE JAVA_SOURCES "android/src/*")
  file(GLOB_RECURSE SDL_JAVA_SOURCES "${SHARDS_DIR}/deps/SDL/android-project/app/src/main/java/*")
  add_android_package(
      NAME virtual_ui_demo
      SOURCES ${JAVA_SOURCES} ${SDL_JAVA_SOURCES}
      MANIFEST android/AndroidManifest.xml
      RES_PATH android/res
      LIB_TARGETS virtual_ui_demo_lib
  )
else()
  set(TARGET_NAME virtual_ui_demo)
  add_executable(virtual_ui_demo ${SOURCES})

  set_target_properties(
    virtual_ui_demo
    PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "Shards Virtual UI Demo"
    MACOSX_BUNDLE_BUNDLE_NAME "xyz.fragcolor.virtual_ui_demo"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
  )
endif()

target_link_libraries(${TARGET_NAME} virtual_ui virtual_ui_test_elements input linalg spdlog SDL2main)
