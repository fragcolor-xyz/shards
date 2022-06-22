
set(FEATURES)

option(BUILD_EGUI_TEST "Build internal egui tests" ON)
if(BUILD_EGUI_TEST)
  list(APPEND FEATURES egui_test)
endif()

add_library(gfx_egui_interop
  egui_interop.cpp
)
target_link_libraries(gfx_egui_interop gfx)

add_rust_library(
  NAME egui-gfx
  FEATURES ${FEATURES}
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS egui_interop.hpp
)

if(BUILD_EGUI_TEST)
  add_executable(gfx-egui-test test.cpp)
  target_link_libraries(gfx-egui-test egui-gfx-rust gfx_egui_interop)
endif()