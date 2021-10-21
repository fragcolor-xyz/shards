# Temporarily disable CMAKE_FIND_ROOT_PATH_MODE_INCLUDE to find boost install outside of ios/emscripten system root (headers only)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE_PREV ${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE})
unset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)

if(IOS)
  find_package(Boost REQUIRED)

  # Boost-context implementation
  enable_language(ASM)
  if(X86 OR X86_IOS_SIMULATOR)
    set(BOOST_CONTEXT_SOURCES
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/posix/stack_traits.cpp
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/jump_x86_64_sysv_macho_gas.S
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/make_x86_64_sysv_macho_gas.S
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/ontop_x86_64_sysv_macho_gas.S
    )
  else()
    set(BOOST_CONTEXT_SOURCES
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/posix/stack_traits.cpp
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/jump_arm64_aapcs_macho_gas.S
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/make_arm64_aapcs_macho_gas.S
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/ontop_arm64_aapcs_macho_gas.S
    )
  endif()
  add_library(boost-context STATIC ${BOOST_CONTEXT_SOURCES})
  target_include_directories(boost-context PRIVATE ${Boost_INCLUDE_DIRS})
elseif(EMSCRIPTEN)
  find_package(Boost REQUIRED)
else()
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(Boost_USE_STATIC_LIBS ON)
  endif()
  
  find_package(Boost REQUIRED COMPONENTS context filesystem)
endif()

# Restore CMAKE_FIND_ROOT_PATH_MODE_INCLUDE
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE_PREV})
