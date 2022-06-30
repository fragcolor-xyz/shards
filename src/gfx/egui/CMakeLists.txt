
set(FEATURES)

option(BUILD_STANDALONE_EGUI_TEST "Build standalone egui+gfx test" ON)
if(BUILD_STANDALONE_EGUI_TEST)
  list(APPEND FEATURES standalone_test)
endif()

add_library(gfx_egui
  egui_types.cpp
  renderer.cpp
)
target_link_libraries(gfx_egui gfx)

add_rust_library(
  NAME egui-gfx
  FEATURES ${FEATURES}
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS rust_interop.hpp
)

if(BUILD_EGUI_TEST)
  add_executable(gfx-egui-test test.cpp)
  target_link_libraries(gfx-egui-test egui-gfx-rust gfx_egui)
endif()