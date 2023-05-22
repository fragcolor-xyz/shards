add_library(input input.cpp input_stack.cpp window_mapping.cpp)
target_include_directories(input PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(input SDL2 linalg spdlog)
target_link_libraries(input gfx) # For window mapping
target_compile_features(input PUBLIC cxx_std_20)

add_executable(test-input tests/tests.cpp)
target_link_libraries(test-input input Catch2Main Catch2 linalg spdlog)
target_compile_features(test-input PUBLIC cxx_std_20)
