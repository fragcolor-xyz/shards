include(Assets)

if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

add_executable(test_gfx
img_compare.cpp
  test_context.cpp
  test_shaderc.cpp
  test_draw.cpp
  test_feature.cpp
  main.cpp
  utils.cpp
)

set(GFX_TEST_DATA_PATH ${CMAKE_CURRENT_SOURCE_DIR}/data)

target_link_libraries(test_gfx gfx Catch2WithMain Catch2)
target_precompile_headers(test_gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")
bundle_asset_path(test_gfx ${GFX_TEST_DATA_PATH}/asm.js)
bundle_asset_path(test_gfx ${CMAKE_CURRENT_SOURCE_DIR}/assets)
add_asset_path_definition(test_gfx PUBLIC GFX_TEST_DATA_PATH ${GFX_TEST_DATA_PATH})

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
