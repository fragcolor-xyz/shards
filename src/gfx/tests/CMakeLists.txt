if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

add_executable(test_gfx 
  test_context.cpp
  test_shaderc.cpp
)
target_link_libraries(test_gfx gfx Catch2WithMain Catch2) 
target_precompile_headers(test_gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")


add_executable(gfx-sandbox sandbox.cpp)
target_link_libraries(gfx-sandbox gfx)

if(EMSCRIPTEN)
	target_link_options(test_gfx PUBLIC --emrun)
	target_link_options(gfx-sandbox PUBLIC --emrun)

	add_executable(gfx-web-sandbox web-sandbox.cpp)
	target_link_libraries(gfx-web-sandbox gfx)
	target_link_options(gfx-web-sandbox PUBLIC 
		"SHELL:-s \"EXPORTED_FUNCTIONS=['_gfx_init', '_gfx_tick']\""
		"SHELL:-s \"EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']\""
	)
endif()
