include(Assets)

if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

add_executable(gfx-sandbox sandbox.cpp)
target_link_libraries(gfx-sandbox gfx gfx_texture_file gfx_imgui gfx_gltf input)

add_executable(test_gfx
  main.cpp
  test_general.cpp
  test_context_data.cpp
  test_shader.cpp
  test_gltf.cpp
  test_helpers.cpp
)

set(GFX_TEST_DATA_PATH ${CMAKE_CURRENT_SOURCE_DIR}/data)

add_library(gfx_test_utils
  platform_id.cpp
  context.cpp
  renderer.cpp
  data.cpp)
target_link_libraries(gfx_test_utils gfx gfx_texture_file Catch2)
add_asset_path_definition(gfx_test_utils PRIVATE GFX_TEST_DATA_PATH ${GFX_TEST_DATA_PATH})

target_link_libraries(test_gfx gfx gfx_test_utils gfx_texture_file gfx_gltf Catch2Main Catch2)
target_precompile_headers(test_gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")
add_asset_path_definition(test_gfx PUBLIC GFX_TEST_DATA_PATH ${GFX_TEST_DATA_PATH})
