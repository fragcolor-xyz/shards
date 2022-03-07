include(Assets)

if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

set(GFX_TEST_DATA_PATH ${CMAKE_CURRENT_SOURCE_DIR}/data)

add_executable(gfx-sandbox sandbox.cpp)
target_link_libraries(gfx-sandbox gfx)
