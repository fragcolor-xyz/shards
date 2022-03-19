include(ExternalProject)

# Inherit build type
set(EXTERNAL_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

# Clear
set(EXTERNAL_CMAKE_ARGS)

# Toolchain file
if(CMAKE_TOOLCHAIN_FILE)
  list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

if(EMSCRIPTEN)
  if(EMSCRIPTEN_PTHREADS)
    list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_CXX_FLAGS=-sUSE_PTHREADS=1 -DCMAKE_C_FLAGS=-sUSE_PTHREADS=1)
  endif()
endif()

# Compiler to use
list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER})
list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})

# Android flags
if(ANDROID)
  list(APPEND EXTERNAL_CMAKE_ARGS -DANDROID_ABI=${ANDROID_ABI})
  list(APPEND EXTERNAL_CMAKE_ARGS -DANDROID_PLATFORM=${ANDROID_PLATFORM})
endif()

# Cross-compile flags
list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR})
list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})

message(STATUS "External Project cmake args: ${EXTERNAL_CMAKE_ARGS}")

# Xcode specific settings
if(CMAKE_GENERATOR STREQUAL Xcode)
  option(XCODE_SDK "The sdk to pass to xcode builds for external project, should match the one passed to the root project build")

  if(NOT XCODE_SDK)
    message(FATAL_ERROR "XCODE_SDK should be set to the sdk being built with\nAs in: cmake --build . -- -sdk <sdk>")
  endif()

  list(APPEND XCODE_ARGS -sdk ${XCODE_SDK} -arch ${CMAKE_SYSTEM_PROCESSOR})
  set(XCODE_CONFIG_BINARY_DIR "${EXTERNAL_BUILD_TYPE}-${XCODE_SDK}") # Debug-iphone, Release-iphonesimulator etc.
  set(XCODE TRUE)
endif()

# Utility function for adding external projects that generate static libraries
function(cb_add_external_project)
  set(OPTS INSTALL)
  set(ARGS NAME LIB_SUFFIX)
  set(MULTI_ARGS TARGETS CMAKE_ARGS REPO_ARGS RELATIVE_INCLUDE_PATHS)
  cmake_parse_arguments(PROJ "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})
  message(STATUS "cb_add_external_project(${PROJ_NAME})")
  message(STATUS "  TARGETS: ${PROJ_TARGETS}")
  message(STATUS "  INSTALL: ${PROJ_INSTALL}")
  message(STATUS "  LIB_SUFFIX: ${PROJ_LIB_SUFFIX}")
  message(STATUS "  CMAKE_ARGS: ${PROJ_CMAKE_ARGS}")
  message(STATUS "  REPO_ARGS: ${PROJ_REPO_ARGS}")
  message(STATUS "  RELATIVE_INCLUDE_PATHS: ${PROJ_RELATIVE_INCLUDE_PATHS}")

  set(VAR_NAME_BASE "${PROJ_NAME}_")

  set(PROJ_PREFIX_PATH ${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME})
  set(PROJ_INSTALL_PATH ${PROJ_PREFIX_PATH})
  message(STATUS "  PROJ_PREFIX_PATH: ${PROJ_PREFIX_PATH}")

  set(PROJ_BINARY_DIR ${PROJ_PREFIX_PATH}/src/${PROJ_NAME}-build)
  if(XCODE)
    set(PROJ_BINARY_CONFIG_DIR ${PROJ_BINARY_DIR}/${XCODE_CONFIG_BINARY_DIR})
  elseif(MSVC)
    set(PROJ_BINARY_CONFIG_DIR ${PROJ_BINARY_DIR}/${EXTERNAL_BUILD_TYPE})
  else()
    set(PROJ_BINARY_CONFIG_DIR ${PROJ_BINARY_DIR})
  endif()
  message(STATUS "  PROJ_BINARY_CONFIG_DIR: ${PROJ_BINARY_CONFIG_DIR}")

  # Resolve generated lib paths
  set(PROJ_LIBS)
  foreach(TARGET ${PROJ_TARGETS})
    if(PROJ_INSTALL)
      set(LIB "${PROJ_INSTALL_PATH}/lib/${LIB_PREFIX}${TARGET}${PROJ_LIB_SUFFIX}${LIB_SUFFIX}")
    else()
      set(LIB "${PROJ_BINARY_CONFIG_DIR}/${LIB_PREFIX}${TARGET}${PROJ_LIB_SUFFIX}${LIB_SUFFIX}")
    endif()
    list(APPEND PROJ_LIBS "${LIB}")
  endforeach()

  if(PROJ_INSTALL)
    set(BUILD_TARGETS --target install)
  else()
    set(BUILD_TARGETS)
    foreach(TARGET ${PROJ_TARGETS})
      list(APPEND BUILD_TARGETS --target ${TARGET})
    endforeach()
  endif()

  # Additional arguments to ExternalProject_Add
  set(EXTPROJ_ARGS)
  message(STATUS "  EXTPROJ_ARGS: ${EXTPROJ_ARGS}")

  if(PROJ_INSTALL)
    set(PROJ_INCLUDE_PATHS ${PROJ_INSTALL_PATH}/include)
  else()
    set(PROJ_INCLUDE_PATHS)
  endif()

  # Append relative include paths
  foreach(REL_INCLUDE_PATH ${PROJ_RELATIVE_INCLUDE_PATHS})
    list(APPEND PROJ_INCLUDE_PATHS ${PROJ_PREFIX_PATH}/src/${PROJ_NAME}/${REL_INCLUDE_PATH})
  endforeach()

  # Touch paths so they exist before building the external project
  foreach(INCLUDE_PATH ${PROJ_INCLUDE_PATHS})
    file(MAKE_DIRECTORY "${INCLUDE_PATH}")
  endforeach()

  ExternalProject_Add(${PROJ_NAME}
    ${PROJ_REPO_ARGS}
    PREFIX ${PROJ_PREFIX_PATH}
    ${EXTPROJ_ARGS}
    INSTALL_COMMAND ""
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} "-DCMAKE_INSTALL_PREFIX:PATH=${PROJ_INSTALL_PATH}" ${EXTERNAL_CMAKE_ARGS} ${PROJ_CMAKE_ARGS}
    BUILD_COMMAND cmake --build . ${BUILD_TARGETS} -- ${XCODE_ARGS}
    BUILD_BYPRODUCTS ${PROJ_LIBS}
  )

  # Create the library targets
  foreach(TARGET_NAME LIB_PATH IN ZIP_LISTS PROJ_TARGETS PROJ_LIBS)
    message(STATUS "  TARGET: ${TARGET_NAME}")
    add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)

    message(STATUS "    IMPORTED_LOCATION: ${LIB_PATH}")
    set_target_properties(${TARGET_NAME} PROPERTIES IMPORTED_LOCATION ${LIB_PATH})

    message(STATUS "    INCLUDE_PATHS: ${PROJ_INCLUDE_PATHS}")
    target_include_directories(${TARGET_NAME} INTERFACE ${PROJ_INCLUDE_PATHS})

    add_dependencies(${TARGET_NAME} ${PROJ_NAME})
  endforeach()
endfunction()
