set(BGFX_DIR ${CHAINBLOCKS_DIR}/deps/bgfx)

option(GFX_DEBUG "Enable context debug mode by default" OFF)
option(GFX_SHADERC_SHARED_LIBRARY "Build shaderc as a shared library" ON)
option(GFX_SHADERC_PREBUILT "When enabled will treat shaderc to be prebuilt" ON)
if(NOT GFX_SHADERC_PREBUILT)
  add_subdirectory(shaderc)
endif()

add_library(gfx 
  context.cpp
  imgui.cpp
  # frame.cpp
  material.cpp
  shader.cpp
  # shader_cache.cpp
#   mesh.cpp
  geom.cpp
  test_platform_id.cpp
  loop.cpp
  frame.cpp
  capture.cpp
  window.cpp
  shaderc.cpp
  paths.cpp
)

target_include_directories(gfx 
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(gfx
  bx bimg bgfx tinygltf
  dear-imgui imgui_club implot imguizmo magic_enum
  spdlog stb
)
target_precompile_headers(gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")

option(GFX_USE_DATA_PATH "Uses build tree paths to find data (dev only)" ON)

if(NOT EMSCRIPTEN AND (CMAKE_BUILD_TYPE MATCHES Debug))
	set(GFX_CAN_USE_DATA_PATH ON)
endif()
if(GFX_CAN_USE_DATA_PATH AND GFX_USE_DATA_PATH)
  target_compile_definitions(gfx PUBLIC GFX_DATA_PATH=\"${CHAINBLOCKS_DIR}\")
endif()

if(NOT GFX_SHADERC_SHARED_LIBRARY AND NOT GFX_SHADERC_PREBUILT)
  target_link_libraries(gfx gfx-shaderc)
  target_compile_definitions(gfx PUBLIC GFX_SHADERC_STATIC=1)
endif()

if(NOT GFX_SHADERC_PREBUILT)
  add_dependencies(gfx gfx-shaderc)
endif()

if(GFX_DEBUG)
	target_compile_definitions(gfx PUBLIC GFX_DEBUG=1)
endif()

if(EMSCRIPTEN)
  # Use builtin SDL2 port
  target_compile_options(gfx PUBLIC 
    "SHELL:-s USE_SDL=2"
  )
  target_link_options(gfx PUBLIC 
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s USE_SDL=2"
	"SHELL:-s ALLOW_MEMORY_GROWTH=1"
  )
else()
  target_link_libraries(gfx SDL2-static)
endif()

list(APPEND GFX_SHADER_PATHS ${BGFX_DIR}/src)
list(APPEND GFX_SHADER_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

set(GFX_GENERATED_INCLUDE_PATH "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${GFX_GENERATED_INCLUDE_PATH}/gfx")
target_include_directories(gfx PUBLIC ${GFX_GENERATED_INCLUDE_PATH})

# Get shader paths relative to the root
foreach(SHADER_PATH ${GFX_SHADER_PATHS})
	file(RELATIVE_PATH SHADER_PATH ${CHAINBLOCKS_DIR} ${SHADER_PATH})
	list(APPEND GFX_SHADER_PATHS_RELATIVE ${SHADER_PATH})
endforeach()

# Write relative shader include paths to shader_paths.hpp
set(GFX_SHADER_PATHS_HEADER "${GFX_GENERATED_INCLUDE_PATH}/gfx/shader_paths.hpp")
include(GenerateC)
file(WRITE ${GFX_SHADER_PATHS_HEADER} "// auto-generated from ${CMAKE_CURRENT_LIST_FILE}\n#pragma once\n\n")
file(APPEND ${GFX_SHADER_PATHS_HEADER} "namespace gfx {\n")
generate_c_string_array(${GFX_SHADER_PATHS_HEADER} "shaderIncludePaths" GFX_SHADER_PATHS_RELATIVE)
file(APPEND ${GFX_SHADER_PATHS_HEADER} "} // namespace gfx\n")

# Bundle shader files on emscripten
if(EMSCRIPTEN)
	foreach(SHADER_PATH ${GFX_SHADER_PATHS})
		file(RELATIVE_PATH SHADER_PATH ${CMAKE_BINARY_DIR} ${SHADER_PATH})
		message(STATUS "--preload-file for shader path: ${SHADER_PATH}")
		target_link_options(gfx PUBLIC "SHELL:--preload-file \"${SHADER_PATH}\"")
	endforeach()
endif()

add_subdirectory(tests)
