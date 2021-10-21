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

if(CHAINBLOCKS_WITH_EXTRA_BLOCKS)
  target_link_libraries(cbl chainblocks-extra)
endif()

if(EMSCRIPTEN)
  if(EMSCRIPTEN_PTHREADS)
    set_target_properties(cbl PROPERTIES SUFFIX "-mt.js")
  else()
    set_target_properties(cbl PROPERTIES SUFFIX "-st.js")
  endif()
endif()