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
add_library(cbl-dll SHARED ${chainblocks_SOURCES} ../core/runtime.cpp)
set_target_properties(cbl-dll PROPERTIES OUTPUT_NAME "cbl")
target_compile_definitions(cbl-dll PUBLIC NO_MAL_MAIN=1 CHAINBLOCKS_CORE_DLL=1 chainblocks_core_EXPORTS=1)
target_link_libraries(cbl-dll 
  chainblocks-core-base
  replxx
)

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