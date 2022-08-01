add_library(input input.cpp)
target_include_directories(input PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(input SDL2-static)
target_compile_features(input PUBLIC cxx_std_20)

add_executable(test_input tests/tests.cpp)
target_link_libraries(test_input input Catch2Main Catch2 linalg spdlog)
target_compile_features(test_input PUBLIC cxx_std_20)
