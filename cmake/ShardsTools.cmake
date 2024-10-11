# Tools that depend on running shards

if(CMAKE_CROSSCOMPILING OR EXTERNAL_SHARDS_COMMAND)
  if(DEFINED ENV{SHARDS_COMMAND})
    message(STATUS "FROM ENV")
    file(REAL_PATH $ENV{SHARDS_COMMAND} SHARDS_COMMAND_ABS)
    set(SHARDS_COMMAND ${SHARDS_COMMAND_ABS} CACHE STRING "The shards interpreter/compiler" FORCE)
  elseif(NOT SHARDS_COMMAND)
    # Find shards when cross compiling since it's needed to precompile scripts
    find_program(SHARDS_COMMAND REQUIRED
      NAMES shards
      DOC "The shards interpreter/compiler"
      PATHS
      ${EXTERNAL_SHARDS_COMMAND}
      ${SHARDS_DIR}/build
    )
  endif()
else()
  set(SHARDS_COMMAND $<TARGET_FILE:shards>)
  set(SHARDS_COMMAND_DEPENDS shards)
endif()

message(STATUS "Using shards command line: ${SHARDS_COMMAND}")

function(shards_build INPUT OUTPUT)
  set(OPTS)
  set(ARGS)
  set(MULTI_ARGS INCLUDE_PATHS)
  cmake_parse_arguments(SHARDS_BUILD "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})

  # Construct a chane of "-Ipath" arguments for include paths
  unset(_TMP_INCLUDE_ARGS)
  foreach(INCLUDE_PATH IN LISTS SHARDS_BUILD_INCLUDE_PATHS)
    list(APPEND _TMP_INCLUDE_ARGS "-I${INCLUDE_PATH}")
  endforeach()
  # string(JOIN " " _TMP_INCLUDE_ARGS ${_TMP_INCLUDE_ARGS})

  # Create a custom command to build the input file
  add_custom_command(
    OUTPUT ${OUTPUT}
    COMMAND ${SHARDS_COMMAND} build ${INPUT} -o ${OUTPUT} -d ${OUTPUT}.d ${_TMP_INCLUDE_ARGS}
    DEPENDS ${INPUT} ${SHARDS_COMMAND_DEPENDS} # Depend on the shards target
    DEPFILE ${OUTPUT}.d
    COMMENT "Building ${INPUT} with Shards"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} # Set the working directory
  )
  set_source_files_properties(${OUTPUT} PROPERTIES GENERATED TRUE)
endfunction()
