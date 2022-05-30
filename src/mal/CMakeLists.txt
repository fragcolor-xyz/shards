set(shards_SOURCES
  stepA_mal.cpp
  Core.cpp
  SHCore.cpp
  Environment.cpp
  Reader.cpp
  ReadLine.cpp
  String.cpp
  Types.cpp
  Validation.cpp
  SHCore.cpp
)

function(setup_shards_target TARGET)
  target_link_libraries(${TARGET} replxx)

  if(SHARDS_WITH_EXTRA_SHARDS)
    target_link_libraries(${TARGET} shards-extra)
  endif()
endfunction()

function(setup_shards_library_target TARGET)
  target_sources(${TARGET} PRIVATE ${shards_SOURCES})
  target_compile_definitions(${TARGET} PUBLIC NO_MAL_MAIN=1)

  setup_shards_target(${TARGET})
endfunction()

# Executable
add_executable(shards ${shards_SOURCES})
target_link_libraries(shards
  shards-core
)
setup_shards_target(shards)

# mal DLL
if(NOT EMSCRIPTEN)
  duplicate_library_target(shards-core-shared SHARED shards-dll)
  setup_shards_library_target(shards-dll)
  set_target_properties(shards-dll PROPERTIES OUTPUT_NAME "shards")
endif()

# mal static library
duplicate_library_target(shards-core-static STATIC shards-static)
setup_shards_library_target(shards-static)
set_target_properties(shards-static PROPERTIES OUTPUT_NAME "${LIB_PREFIX}shards-static")

if(EMSCRIPTEN)
  if(EMSCRIPTEN_PTHREADS)
    set_target_properties(shards PROPERTIES SUFFIX "-mt.js")
  else()
    set_target_properties(shards PROPERTIES SUFFIX "-st.js")
  endif()

  target_link_options(shards PUBLIC
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_NAME=shards"
    "SHELL:-s NO_EXIT_RUNTIME=1"
  )
endif()