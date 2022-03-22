include(ExternalProject)

# Inherit build type
set(EXTERNAL_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

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
  set(OPTS 
    INSTALL)
  set(ARGS 
    NAME 
    LIB_SUFFIX)
  set(MULTI_ARGS
    TARGETS 
    LIB_NAMES
    LIB_RELATIVE_DIRS
    CMAKE_ARGS 
    REPO_ARGS 
    RELATIVE_INCLUDE_PATHS  # Include paths relative to source dir (without INSTALL)
    RELATIVE_BINARY_INCLUDE_PATHS # Include paths relative to binary dir (without INSTALL)
    RELATIVE_INSTALL_INCLUDE_PATHS # Include paths relative to install dir (with INSTALL)
  )
  cmake_parse_arguments(PROJ "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})

  message(STATUS "cb_add_external_project(${PROJ_NAME})")
  message(STATUS "  TARGETS: ${PROJ_TARGETS}")
  message(STATUS "  INSTALL: ${PROJ_INSTALL}")
  message(STATUS "  LIB_SUFFIX: ${PROJ_LIB_SUFFIX}")
  message(STATUS "  LIB_RELATIVE_DIRS: ${PROJ_LIB_RELATIVE_DIRS}")
  message(STATUS "  CMAKE_ARGS: ${PROJ_CMAKE_ARGS}")
  message(STATUS "  REPO_ARGS: ${PROJ_REPO_ARGS}")
  if(PROJ_INSTALL)
    message(STATUS "  RELATIVE_INSTALL_INCLUDE_PATHS: ${PROJ_RELATIVE_INSTALL_INCLUDE_PATHS}")
  else()
    message(STATUS "  RELATIVE_INCLUDE_PATHS: ${PROJ_RELATIVE_INCLUDE_PATHS}")
    message(STATUS "  RELATIVE_BINARY_INCLUDE_PATHS: ${PROJ_RELATIVE_BINARY_INCLUDE_PATHS}")
  endif()

  # Resolve/verify lib names
  if(PROJ_LIB_NAMES)
    list(LENGTH PROJ_LIB_NAMES A)
    list(LENGTH PROJ_TARGETS B)
    if(NOT (A EQUAL B))
      message(FATAL_ERROR "LIB_NAMES should have the same number of elements as TARGETS")
    endif()
  else()
    # Generate lib names from target names
    foreach(TARGET ${PROJ_TARGETS})
      list(APPEND PROJ_LIB_NAMES ${TARGET})
    endforeach()
  endif()
  message(STATUS "  LIB_NAMES: ${LIB_NAMES}")

  set(PROJ_PREFIX_PATH ${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME})
  set(PROJ_INSTALL_PATH ${PROJ_PREFIX_PATH})
  message(STATUS "  PROJ_PREFIX_PATH: ${PROJ_PREFIX_PATH}")

  # Resolve binary output directory when using configuration-specific generators
  set(PROJ_BINARY_DIR ${PROJ_PREFIX_PATH}/src/${PROJ_NAME}-build)
  if(XCODE)
    set(BINARY_CONFIG_DIR ${XCODE_CONFIG_BINARY_DIR}/)
  elseif(MSVC)
    set(BINARY_CONFIG_DIR ${EXTERNAL_BUILD_TYPE}/)
  else()
    set(BINARY_CONFIG_DIR )
  endif()
  message(STATUS "  BINARY_CONFIG_DIR: ${BINARY_CONFIG_DIR}")

  if(NOT PROJ_LIB_RELATIVE_DIRS)
    foreach(TARGET ${PROJ_TARGETS})
      list(APPEND PROJ_LIB_RELATIVE_DIRS "")
    endforeach()
  else()
    list(LENGTH PROJ_LIB_RELATIVE_DIRS A)
    list(LENGTH PROJ_TARGETS B)
    if(NOT (A EQUAL B))
      message(FATAL_ERROR "LIB_RELATIVE_DIRS should have the same number of elements as TARGETS")
    endif()
  endif()

  # Resolve generated lib paths
  set(PROJ_LIBS)
  foreach(LIB_NAME REL_DIR IN ZIP_LISTS PROJ_LIB_NAMES PROJ_LIB_RELATIVE_DIRS)
    if(PROJ_INSTALL)
      set(LIB "${PROJ_INSTALL_PATH}/lib/${REL_DIR}${LIB_PREFIX}${LIB_NAME}${PROJ_LIB_SUFFIX}${LIB_SUFFIX}")
    else()
      set(LIB "${PROJ_BINARY_DIR}/${REL_DIR}${BINARY_CONFIG_DIR}${LIB_PREFIX}${LIB_NAME}${PROJ_LIB_SUFFIX}${LIB_SUFFIX}")
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


  if(PROJ_INSTALL)
    # Append relative (to install path) include paths
    foreach(REL_INSTALL_INCLUDE_PATH ${PROJ_RELATIVE_INSTALL_INCLUDE_PATHS})
      list(APPEND PROJ_INCLUDE_PATHS ${PROJ_INSTALL_PATH}/${REL_INSTALL_INCLUDE_PATH})
    endforeach()
  else()
    # Append relative include paths
    foreach(REL_INCLUDE_PATH ${PROJ_RELATIVE_INCLUDE_PATHS})
      list(APPEND PROJ_INCLUDE_PATHS ${PROJ_PREFIX_PATH}/src/${PROJ_NAME}/${REL_INCLUDE_PATH})
    endforeach()
    foreach(REL_INCLUDE_PATH ${PROJ_RELATIVE_BINARY_INCLUDE_PATHS})
      list(APPEND PROJ_INCLUDE_PATHS ${PROJ_BINARY_DIR}/${REL_INCLUDE_PATH})
    endforeach()
  endif()

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
    BUILD_COMMAND ${CMAKE_COMMAND} --build . ${BUILD_TARGETS} -- ${XCODE_ARGS}
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
