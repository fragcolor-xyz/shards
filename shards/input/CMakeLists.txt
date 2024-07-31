add_library(input
  input_stack.cpp window_mapping.cpp master.cpp
  gestures.cpp clipboard.cpp
  event_decoder.cpp
)
target_include_directories(input PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(input linalg spdlog Boost::lockfree)
if(NOT EMSCRIPTEN)
  target_link_libraries(input SDL3-static)
else()
  target_include_directories(input PUBLIC ${SHARDS_DIR}/deps/SDL3/include)
endif()
target_link_libraries(input gfx-core) # For window mapping
target_compile_features(input PUBLIC cxx_std_20)

add_executable(test-input tests/tests.cpp)
target_link_libraries(test-input input Catch2Main Catch2 linalg spdlog)
target_compile_features(test-input PUBLIC cxx_std_20)

add_rust_library(
  NAME input-debug-ui
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/debug_ui
  DEPENDS debug_ui.hpp)
