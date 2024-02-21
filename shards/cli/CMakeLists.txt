set(cli_SOURCES
  main.cpp
)

function(setup_shards_target TARGET)
  if(NOT EMSCRIPTEN)
    target_link_libraries(${TARGET} Boost::process)
  endif()
  target_link_libraries(${TARGET} shards-lang-rust shards-core)
endfunction()

function(setup_shards_library_target TARGET)
  target_sources(${TARGET} PRIVATE ${cli_SOURCES})
  setup_shards_target(${TARGET})
endfunction()

# Executable
add_executable(shards ${cli_SOURCES})
setup_shards_target(shards)
target_link_libraries(shards shards-union)

# library
add_library(shards-lib)
setup_shards_library_target(shards-lib)
set_target_properties(shards-lib PROPERTIES OUTPUT_NAME "${LIB_PREFIX}shards-static")
target_compile_definitions(shards-lib PRIVATE "shards_EXPORTS=1")

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
