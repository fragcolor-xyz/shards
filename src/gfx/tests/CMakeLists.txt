include(Assets)

if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

add_executable(gfx-sandbox sandbox.cpp)
target_link_libraries(gfx-sandbox gfx)

add_executable(test_gfx
  test_renderer.cpp
  test_main.cpp
)

target_link_libraries(test_gfx gfx Catch2WithMain Catch2)
target_precompile_headers(test_gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")
