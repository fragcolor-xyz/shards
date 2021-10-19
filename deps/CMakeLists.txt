set(BUILD_SHARED_LIBS OFF)
include(ExternalProject)

if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set(EXTERNAL_BUILD_TYPE "MinSizeRel")
else()
  set(EXTERNAL_BUILD_TYPE "Release")
endif()

string(TOUPPER "CMAKE_CXX_FLAGS_${EXTERNAL_BUILD_TYPE}" EXTERNAL_CXX_FLAGS_VAR)
set(EXTERNAL_CXX_FLAGS ${CMAKE_CXX_FLAGS} ${${EXTERNAL_CXX_FLAGS_VAR}})
set(EXTERNAL_CMAKE_ARGS "-DCMAKE_CXX_FLAGS=${EXTERNAL_CXX_FLAGS}")
message(STATUS "External cmake args: ${EXTERNAL_CMAKE_ARGS}")

add_subdirectory(filesystem)
add_subdirectory(json)
add_subdirectory(kcp)
add_subdirectory(magic_enum)
add_subdirectory(nameof)
add_subdirectory(pareto)
add_subdirectory(replxx)
add_subdirectory(spdlog)

set(SDL_BUILD_PATH ${CMAKE_CURRENT_BINARY_DIR}/sdl_a)
if(NOT EMSCRIPTEN)
  if(X86_IOS_SIMULATOR)
    set(SDL_XCODE_BUILD_COMMAND cd Xcode/SDL && xcodebuild -configuration Release -target "Static Library-iOS" -target hidapi-iOS -sdk iphonesimulator -arch x86_64)
    set(SDL_LIB_PATH ${SDL_BUILD_PATH}/src/sdl_a/Xcode/SDL/build/Release-iphonesimulator/libSDL2.a)
  elseif(IOS)  
    set(SDL_XCODE_BUILD_COMMAND cd Xcode/SDL && xcodebuild -configuration Release -target "Static Library-iOS" -target hidapi-iOS -sdk iphoneos -arch arm64)
    set(SDL_LIB_PATH ${SDL_BUILD_PATH}/src/sdl_a/Xcode/SDL/build/Release-iphoneos/libSDL2.a)
  endif()
  
  if(SDL_XCODE_BUILD_COMMAND)
    ExternalProject_Add(sdl_a
      URL ${CMAKE_CURRENT_SOURCE_DIR}/SDL
      PREFIX ${SDL_BUILD_PATH}
      CONFIGURE_COMMAND ""
      BUILD_IN_SOURCE true
      BUILD_COMMAND ${SDL_XCODE_BUILD_COMMAND}
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS ${SDL_LIB_PATH}
    )
  else()
    if(WIN32)
      set(SDL_LIB_PATH ${SDL_BUILD_PATH}/lib/libSDL2-static.a)
    else()
      set(SDL_LIB_PATH ${SDL_BUILD_PATH}/lib/libSDL2.a)
    endif()
    
    ExternalProject_Add(sdl_a
      URL ${CMAKE_CURRENT_SOURCE_DIR}/SDL
      PREFIX ${SDL_BUILD_PATH}
      CMAKE_ARGS -DSDL_RENDER_DISABLED=ON -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX:PATH=${SDL_BUILD_PATH} ${EXTERNAL_CMAKE_ARGS}
      BUILD_BYPRODUCTS ${SDL_LIB_PATH}
    )
  endif()
  
  add_library(SDL2-static STATIC IMPORTED GLOBAL)
  set_target_properties(SDL2-static PROPERTIES IMPORTED_LOCATION ${SDL_LIB_PATH})
  
  file(MAKE_DIRECTORY ${SDL_BUILD_PATH}/include/SDL2)
  target_include_directories(SDL2-static INTERFACE ${SDL_BUILD_PATH}/include ${SDL_BUILD_PATH}/include/SDL2)
  add_dependencies(SDL2-static sdl_a)
  
  if(WIN32)
    target_link_libraries(SDL2-static INTERFACE Winmm Imm32 Setupapi Version)
  endif()
endif()

if(LINUX AND NOT EMSCRIPTEN)
  # NOTE: Build externally since it only behaves correctly when built in Release and without certain flags defined in Platform.cmake (linux)
  ExternalProject_Add(wasm3-build
    URL ${CHAINBLOCKS_DIR}/deps/wasm3
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/wasm3-deps
    BUILD_IN_SOURCE True
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DBUILD_NATIVE=0
    BUILD_COMMAND cmake --build . --target m3
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/wasm3-deps/src/wasm3-build/source/libm3.a
  )

  add_library(m3 STATIC IMPORTED GLOBAL)
  set_target_properties(m3 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/wasm3-deps/src/wasm3-build/source/libm3.a)
  target_include_directories(m3 INTERFACE ${CHAINBLOCKS_DIR}/deps/wasm3/source)
  add_dependencies(m3 wasm3-build)
else()
  option(BUILD_NATIVE "" OFF)
  add_subdirectory(wasm3)
endif()

add_subdirectory(Catch2)

option(SNAPPY_BUILD_TESTS OFF "")
add_subdirectory(snappy)

set(BROTLI_DISABLE_TESTS ON)
add_subdirectory(brotli)

option(TINYGLTF_BUILD_LOADER_EXAMPLE OFF "") 
add_subdirectory(tinygltf)

option(TF_BUILD_TESTS "" OFF)
option(TF_BUILD_EXAMPLES "" OFF)
add_subdirectory(cpp-taskflow)

option(KISSFFT_STATIC "" ON)
option(KISSFFT_TEST "" OFF)
option(KISSFFT_TOOLS "" OFF)
add_subdirectory(kissfft)

option(BGFX_BUILD_EXAMPLES "" OFF)
option(BGFX_INSTALL "" OFF)
option(BIMG_DECODE_ENABLE "" OFF)
set(BX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bx)
set(BIMG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bimg)
set(BGFX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bgfx)
add_subdirectory(bgfx.cmake)

add_library(bgfx-example-common INTERFACE)
target_include_directories(bgfx-example-common INTERFACE ${BGFX_DIR}/examples/common/imgui)

add_library(xxHash INTERFACE)
target_include_directories(xxHash INTERFACE xxHash)

add_library(imgui_club INTERFACE)
target_include_directories(imgui_club INTERFACE imgui_club/imgui_memory_editor)

set(imguizmo_SOURCES 
  imguizmo/ImCurveEdit.cpp
  imguizmo/ImGradient.cpp
  imguizmo/ImGuizmo.cpp
  imguizmo/ImSequencer.cpp
)
add_library(imguizmo STATIC ${imguizmo_SOURCES})
target_include_directories(imguizmo PUBLIC imguizmo)
target_include_directories(imguizmo PRIVATE bgfx/3rdparty/dear-imgui)
target_link_libraries(imguizmo PUBLIC dear-imgui)

add_library(implot STATIC 
  implot/implot.cpp
  implot/implot_items.cpp
)
target_include_directories(implot PUBLIC implot)
target_link_libraries(implot
  PUBLIC dear-imgui
  PRIVATE stb
)

add_library(linalg INTERFACE)
target_include_directories(linalg INTERFACE linalg)

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE miniaudio)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb)

add_library(utf8.h INTERFACE)
target_include_directories(utf8.h INTERFACE utf8.h)

add_library(pdqsort INTERFACE) 
target_include_directories(pdqsort INTERFACE pdqsort)
