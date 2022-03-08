set(BUILD_SHARED_LIBS OFF)

# Find & cache git using (only considering host tools)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH ON)
find_package(Git REQUIRED)

include(ExternalProject)

set(EXTERNAL_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})

if(CMAKE_TOOLCHAIN_FILE)
  list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

if(EMSCRIPTEN)
  list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
  if(EMSCRIPTEN_PTHREADS)
    list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_CXX_FLAGS=-sUSE_PTHREADS=1 -DCMAKE_C_FLAGS=-sUSE_PTHREADS=1)
  endif()
elseif(IOS)
  list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})
endif()

list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER})
list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})

if(IOS)
  if(X86_IOS_SIMULATOR)
  list(APPEND EXTRA_CMAKE_BUILD_ARGS -- -sdk iphonesimulator -arch x86_64)
    if(EXTERNAL_BUILD_TYPE STREQUAL "Debug")
      set(EXTRA_LIBS_DIR "Debug-iphonesimulator")
    else()
      set(EXTRA_LIBS_DIR "Release-iphonesimulator")
    endif()
  else()
    list(APPEND EXTRA_CMAKE_BUILD_ARGS -- -sdk iphoneos -arch arm64)
    if(EXTERNAL_BUILD_TYPE STREQUAL "Debug")
      set(EXTRA_LIBS_DIR "Debug-iphoneos")
    else()
      set(EXTRA_LIBS_DIR "Release-iphoneos")
    endif()
  endif()
endif()

message(STATUS "External cmake args: ${EXTERNAL_CMAKE_ARGS}")

add_subdirectory(filesystem)
add_subdirectory(json)
add_subdirectory(kcp)
add_subdirectory(magic_enum)
add_subdirectory(nameof)
add_subdirectory(pareto)
add_subdirectory(replxx)
add_subdirectory(spdlog)
add_subdirectory(boost)

set(SDL_BUILD_PATH ${CMAKE_CURRENT_BINARY_DIR}/sdl_a)
if(NOT EMSCRIPTEN)
  if(SDL_XCODE_BUILD_COMMAND)
    if(X86_IOS_SIMULATOR)
      set(SDL_XCODE_BUILD_COMMAND cd Xcode/SDL && xcodebuild -configuration Release -target "Static Library-iOS" -sdk iphonesimulator -arch x86_64)
      set(SDL_LIB_PATH ${SDL_BUILD_PATH}/src/sdl_a/Xcode/SDL/build/Release-iphonesimulator/libSDL2.a)
    elseif(IOS)
      set(SDL_XCODE_BUILD_COMMAND cd Xcode/SDL && xcodebuild -configuration Release -target "Static Library-iOS" -sdk iphoneos -arch arm64)
      set(SDL_LIB_PATH ${SDL_BUILD_PATH}/src/sdl_a/Xcode/SDL/build/Release-iphoneos/libSDL2.a)
    endif()

    set(SDL_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDL/include)

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
    set(SDL_INCLUDE_PATH ${SDL_BUILD_PATH}/include ${SDL_BUILD_PATH}/include/SDL2)

    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      set(SDL_LIB_SUFFIX "d")
    endif()
    set(SDL_LIB_PATH ${SDL_BUILD_PATH}/lib/${LIB_PREFIX}SDL2${SDL_LIB_SUFFIX}${LIB_SUFFIX})

    ExternalProject_Add(sdl_a
      URL ${CMAKE_CURRENT_SOURCE_DIR}/SDL
      PREFIX ${SDL_BUILD_PATH}
      CMAKE_ARGS -DSDL_RENDER_DISABLED=ON -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} "-DCMAKE_INSTALL_PREFIX:PATH=${SDL_BUILD_PATH}" ${EXTERNAL_CMAKE_ARGS}
      BUILD_BYPRODUCTS ${SDL_LIB_PATH}
    )
  endif()

  add_library(SDL2-static STATIC IMPORTED GLOBAL)
  set_target_properties(SDL2-static PROPERTIES IMPORTED_LOCATION ${SDL_LIB_PATH})

  file(MAKE_DIRECTORY ${SDL_BUILD_PATH}/include/SDL2)
  target_include_directories(SDL2-static INTERFACE ${SDL_INCLUDE_PATH})
  add_dependencies(SDL2-static sdl_a)

  if(WIN32)
    target_link_libraries(SDL2-static INTERFACE Winmm Imm32 Setupapi Version)
  endif()
endif()

set(M3_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/wasm3_a/src/wasm3_a/source/${LIB_PREFIX}m3${LIB_SUFFIX})
ExternalProject_Add(wasm3_a
  URL ${CHAINBLOCKS_DIR}/deps/wasm3
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/wasm3_a
  BUILD_IN_SOURCE True
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DBUILD_NATIVE=0 ${EXTERNAL_CMAKE_ARGS}
  BUILD_COMMAND cmake --build . --target m3
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${M3_LIB_PATH}
)

add_library(m3 STATIC IMPORTED GLOBAL)
set_target_properties(m3 PROPERTIES IMPORTED_LOCATION ${M3_LIB_PATH})
target_include_directories(m3 INTERFACE ${CHAINBLOCKS_DIR}/deps/wasm3/source)
add_dependencies(m3 wasm3_a)

set(SNAPPY_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/lib/${LIB_PREFIX}snappy${LIB_SUFFIX})
ExternalProject_Add(snappy_a
  GIT_REPOSITORY    https://github.com/chainblocks/snappy.git
  GIT_TAG           563e4e90f4ed6314a14055826f027b2239a8bf0e
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/snappy_a
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} -DSNAPPY_BUILD_TESTS=0 -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/snappy_a ${EXTERNAL_CMAKE_ARGS}
  BUILD_COMMAND cmake --build . --target snappy ${EXTRA_CMAKE_BUILD_ARGS}
  BUILD_BYPRODUCTS ${SNAPPY_LIB_PATH}
  BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/include/snappy.h
)
add_library(snappy STATIC IMPORTED GLOBAL)
set_target_properties(snappy PROPERTIES IMPORTED_LOCATION ${SNAPPY_LIB_PATH})
target_include_directories(snappy INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/include)
add_dependencies(snappy snappy_a)

if(IOS)
  set(BROTLICOMMON_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${EXTRA_LIBS_DIR}/${LIB_PREFIX}brotlicommon-static${LIB_SUFFIX})
  set(BROTLIDEC_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${EXTRA_LIBS_DIR}/${LIB_PREFIX}brotlidec-static${LIB_SUFFIX})
  set(BROTLIENC_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${EXTRA_LIBS_DIR}/${LIB_PREFIX}brotlienc-static${LIB_SUFFIX})
else()
  set(BROTLICOMMON_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${LIB_PREFIX}brotlicommon-static${LIB_SUFFIX})
  set(BROTLIDEC_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${LIB_PREFIX}brotlidec-static${LIB_SUFFIX})
  set(BROTLIENC_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${LIB_PREFIX}brotlienc-static${LIB_SUFFIX})
endif()

ExternalProject_Add(brotli_a
  GIT_REPOSITORY    https://github.com/chainblocks/brotli.git
  GIT_TAG           e83c7b8e8fb8b696a1df6866bc46cbb76d7e0348
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/brotli_a
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} -DBROTLI_BUNDLED_MODE=1 "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/brotli_a" ${EXTERNAL_CMAKE_ARGS}
  BUILD_COMMAND cmake --build . --target brotlicommon-static brotlidec-static brotlienc-static ${EXTRA_CMAKE_BUILD_ARGS}
  BUILD_BYPRODUCTS ${BROTLICOMMON_LIB_PATH}
  BUILD_BYPRODUCTS ${BROTLIDEC_LIB_PATH}
  BUILD_BYPRODUCTS ${BROTLIENC_LIB_PATH}
  BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a/c/include/brotli/decode.h
  INSTALL_COMMAND ""
)

add_library(brotlicommon-static STATIC IMPORTED GLOBAL)
target_include_directories(brotlicommon-static INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a/c/include)
add_dependencies(brotlicommon-static brotli_a)
set_target_properties(brotlicommon-static PROPERTIES
    IMPORTED_LOCATION ${BROTLICOMMON_LIB_PATH}
)

add_library(brotlidec-static STATIC IMPORTED GLOBAL)
target_include_directories(brotlidec-static INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a/c/include)
add_dependencies(brotlidec-static brotli_a)
set_target_properties(brotlidec-static PROPERTIES
  IMPORTED_LOCATION ${BROTLIDEC_LIB_PATH}
)

add_library(brotlienc-static STATIC IMPORTED GLOBAL)
target_include_directories(brotlienc-static INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a/c/include)
add_dependencies(brotlienc-static brotli_a)
set_target_properties(brotlienc-static PROPERTIES
  IMPORTED_LOCATION ${BROTLIENC_LIB_PATH}
)

set(CATCH2_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/catch2_a/lib/${LIB_PREFIX}Catch2${LIB_SUFFIX})
set(CATCH2MAIN_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/catch2_a/lib/${LIB_PREFIX}Catch2Main${LIB_SUFFIX})
ExternalProject_Add(catch2_a
  URL ${CHAINBLOCKS_DIR}/deps/Catch2
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/catch2_a
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/catch2_a ${EXTERNAL_CMAKE_ARGS}
  BUILD_BYPRODUCTS ${CATCH2_LIB_PATH}
  BUILD_BYPRODUCTS ${CATCH2MAIN_LIB_PATH}
)

add_library(Catch2 STATIC IMPORTED GLOBAL)
set_target_properties(Catch2 PROPERTIES IMPORTED_LOCATION ${CATCH2_LIB_PATH})
target_include_directories(Catch2 INTERFACE ${CHAINBLOCKS_DIR}/deps/Catch2/src)
add_dependencies(Catch2 catch2_a)

add_library(Catch2WithMain STATIC IMPORTED GLOBAL)
set_target_properties(Catch2WithMain PROPERTIES IMPORTED_LOCATION ${CATCH2MAIN_LIB_PATH})
target_include_directories(Catch2WithMain INTERFACE ${CHAINBLOCKS_DIR}/deps/Catch2/src)
add_dependencies(Catch2WithMain catch2_a)

set(KISSFFT_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/kissfft_a/src/kissfft_a/${LIB_PREFIX}kissfft-float${LIB_SUFFIX})
ExternalProject_Add(kissfft_a
  URL ${CHAINBLOCKS_DIR}/deps/kissfft
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/kissfft_a
  BUILD_IN_SOURCE True
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} ${EXTERNAL_CMAKE_ARGS} -DKISSFFT_ARCH_FLAGS=${ARCH_FLAGS} -DKISSFFT_PKGCONFIG=0 -DKISSFFT_STATIC=1 -DKISSFFT_TEST=0 -DKISSFFT_TOOLS=0
  BUILD_COMMAND cmake --build .
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${KISSFFT_LIB_PATH}
)

add_library(kissfft STATIC IMPORTED GLOBAL)
set_target_properties(kissfft PROPERTIES IMPORTED_LOCATION ${KISSFFT_LIB_PATH})
target_include_directories(kissfft INTERFACE ${CHAINBLOCKS_DIR}/deps/kissfft)
add_dependencies(kissfft kissfft_a)

add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

option(TF_BUILD_TESTS "" OFF)
option(TF_BUILD_EXAMPLES "" OFF)
add_subdirectory(cpp-taskflow)

add_library(xxHash INTERFACE)
target_include_directories(xxHash INTERFACE xxHash)

add_library(imgui STATIC
  imgui/imgui.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_draw.cpp
)
target_include_directories(imgui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} imgui)

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
target_link_libraries(imguizmo PUBLIC imgui)

add_library(implot STATIC
  implot/implot.cpp
  implot/implot_items.cpp
)
target_include_directories(implot PUBLIC implot)
target_link_libraries(implot
  PUBLIC imgui
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
