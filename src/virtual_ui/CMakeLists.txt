add_library(virtual-ui context.cpp panel.cpp)
target_include_directories(virtual-ui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(virtual-ui SDL2 gfx)
target_compile_features(virtual-ui PUBLIC cxx_std_20)

add_rust_library(
  NAME test-elements
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/test_elements
  TARGET_NAME virtual-ui-test-elements)
target_link_libraries(virtual-ui-test-elements INTERFACE gfx-egui)

add_executable(test-virtual-ui tests/tests.cpp)
target_link_libraries(test-virtual-ui virtual-ui-test-elements input gfx-test-utils Catch2Main Catch2 linalg spdlog)
target_compile_features(test-virtual-ui PUBLIC cxx_std_20)

add_subdirectory(demo)
