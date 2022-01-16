set(chainblocks_SOURCES
  stepA_mal.cpp
  Core.cpp
  CBCore.cpp
  Environment.cpp
  Reader.cpp
  ReadLine.cpp
  String.cpp
  Types.cpp
  Validation.cpp
  CBCore.cpp
)

function(setup_cbl_target TARGET)
  target_link_libraries(${TARGET} replxx)

  if(CHAINBLOCKS_WITH_EXTRA_BLOCKS)
    target_link_libraries(${TARGET} chainblocks-extra)
  endif()
endfunction()

function(setup_cbl_library_target TARGET)
  target_sources(${TARGET} PRIVATE ${chainblocks_SOURCES})
  target_compile_definitions(${TARGET} PUBLIC NO_MAL_MAIN=1)

  setup_cbl_target(${TARGET})
endfunction()

# Executable
add_executable(cbl ${chainblocks_SOURCES})
target_link_libraries(cbl
  chainblocks-core
)
setup_cbl_target(cbl)

# mal DLL
if(NOT EMSCRIPTEN)
  duplicate_library_target(chainblocks-core-shared SHARED cbl-dll)
  setup_cbl_library_target(cbl-dll)
  set_target_properties(cbl-dll PROPERTIES OUTPUT_NAME "cbl")
endif()

# mal static library
duplicate_library_target(chainblocks-core-static STATIC cbl-static)
setup_cbl_library_target(cbl-static)
set_target_properties(cbl-static PROPERTIES OUTPUT_NAME "${LIB_PREFIX}cbl-static")

if(EMSCRIPTEN)
  if(EMSCRIPTEN_PTHREADS)
    set_target_properties(cbl PROPERTIES SUFFIX "-mt.js")
  else()
    set_target_properties(cbl PROPERTIES SUFFIX "-st.js")
  endif()
endif()