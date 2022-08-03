add_library(virtual_ui dummy.cpp)
target_include_directories(virtual_ui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(virtual_ui SDL2-static)
target_compile_features(virtual_ui PUBLIC cxx_std_20)

add_rust_library(
  NAME virtual_ui
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/virtual_ui)

add_executable(test_virtual_ui tests/tests.cpp)
target_link_libraries(test_virtual_ui virtual_ui-rust input gfx_test_utils Catch2Main Catch2 linalg spdlog)
target_compile_features(test_virtual_ui PUBLIC cxx_std_20)
