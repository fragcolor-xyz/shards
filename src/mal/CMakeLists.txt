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

add_executable(cbl ${chainblocks_SOURCES})
target_link_libraries(cbl
  chainblocks-core
  replxx
)

# mal DLL
duplicate_library_target(chainblocks-core-shared SHARED cbl-dll)
target_sources(cbl-dll PRIVATE ${chainblocks_SOURCES})
set_target_properties(cbl-dll PROPERTIES OUTPUT_NAME "cbl")
target_compile_definitions(cbl-dll PUBLIC NO_MAL_MAIN=1)
target_link_libraries(cbl-dll replxx)

if(CHAINBLOCKS_WITH_EXTRA_BLOCKS)
  target_link_libraries(cbl chainblocks-extra)
  target_link_libraries(cbl-dll chainblocks-extra)
endif()

if(EMSCRIPTEN)
  if(EMSCRIPTEN_PTHREADS)
    set_target_properties(cbl PROPERTIES SUFFIX "-mt.js")
  else()
    set_target_properties(cbl PROPERTIES SUFFIX "-st.js")
  endif()
endif()