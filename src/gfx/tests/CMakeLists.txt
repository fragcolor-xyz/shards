include(Assets)

if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

add_executable(gfx-sandbox sandbox.cpp)
target_link_libraries(gfx-sandbox gfx gfx-texture-file gfx-imgui gfx-gltf shards-logging input)

add_executable(test-gfx
  main.cpp
  test_general.cpp
  test_context_data.cpp
  test_shader.cpp
  test_gltf.cpp
  test_gizmos.cpp
  test_comparison.cpp
)

set(GFX_TEST_DATA_PATH ${CMAKE_CURRENT_SOURCE_DIR}/data)

add_library(gfx-test-utils
  platform_id.cpp
  context.cpp
  renderer.cpp
  data.cpp)
target_link_libraries(gfx-test-utils gfx gfx-texture-file Catch2)
add_asset_path_definition(gfx-test-utils PRIVATE GFX_TEST_DATA_PATH ${GFX_TEST_DATA_PATH})

target_link_libraries(test-gfx gfx gfx-test-utils gfx-texture-file gfx-gltf Catch2Main Catch2)
target_precompile_headers(test-gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")
add_asset_path_definition(test-gfx PUBLIC GFX_TEST_DATA_PATH ${GFX_TEST_DATA_PATH})
